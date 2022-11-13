#include "audio_manager.hpp"
#include "client.pb.h"
#include "common.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <functional>

#include <combaseapi.h>
#include <atlbase.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <Audiopolicy.h>

// 利用RAII手法，自动调用 CoUninitialize
class CoInitializeGuard {
public:
    CoInitializeGuard()
    {
        _hr = CoInitializeEx(nullptr, COINIT::COINIT_MULTITHREADED);
    }

    ~CoInitializeGuard()
    {
        if (_hr == S_OK || _hr == S_FALSE) {
            CoUninitialize();
        }
    }

    HRESULT result() const { return _hr; }

private:
    HRESULT _hr;
};

constexpr inline void exit_on_failed(HRESULT hr);
void printEndpoints(CComPtr<IMMDeviceCollection> pColletion);
std::string wchars_to_mbs(const wchar_t* s);

audio_manager::audio_manager()
{
}

audio_manager::~audio_manager()
{
}

asio::awaitable<void> audio_manager::do_loopback_recording(asio::io_context& ioc, const std::wstring& endpoint_id)
{
    HRESULT hr{};

    CoInitializeGuard coInitializeGuard;
    exit_on_failed(coInitializeGuard.result());

    CComPtr<IMMDeviceEnumerator> pEnumerator;
    hr = pEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exit_on_failed(hr);


    CComPtr<IMMDevice> pEndpoint;
    hr = pEnumerator->GetDevice(endpoint_id.c_str(), &pEndpoint);
    exit_on_failed(hr);

    CComPtr<IPropertyStore> pProps;
    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
    exit_on_failed(hr);
    PROPVARIANT varName;
    PropVariantInit(&varName);
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    exit_on_failed(hr);
    std::cout << "select audio endpoint: " << wchars_to_mbs(varName.pwszVal) << std::endl;
    PropVariantClear(&varName);

    CComPtr<IAudioClient> pAudioClient;
    hr = pEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    exit_on_failed(hr);

    CComHeapPtr<WAVEFORMATEX> pDeviceFormat;
    pAudioClient->GetMixFormat(&pDeviceFormat);

    this->set_format(pDeviceFormat);

    constexpr int REFTIMES_PER_SEC = 10000000;      // 1 reference_time = 100ns
    constexpr int REFTIMES_PER_MILLISEC = 10000;

    const REFERENCE_TIME hnsRequestedDuration = 1 * REFTIMES_PER_SEC; // 1s
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pDeviceFormat, nullptr);
    exit_on_failed(hr);

    UINT32 bufferFrameCount{};
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    exit_on_failed(hr);

    CComPtr<IAudioCaptureClient> pCaptureClient;
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    exit_on_failed(hr);

    hr = pAudioClient->Start();
    exit_on_failed(hr);

    const REFERENCE_TIME hnsActualDuration = (long long)REFTIMES_PER_SEC * bufferFrameCount / pDeviceFormat->nSamplesPerSec;

    UINT32 data_ckSize = 0;
    UINT32 frame_count = 0;

    int seconds{};

    using namespace std::literals;
    asio::steady_timer timer(ioc);
    timer.expires_at(std::chrono::steady_clock::now());

    do {
        timer.expires_at(timer.expiry() + 1ms);
        auto [ec] = co_await timer.async_wait(co_token);
        if (ec) {
            break;
        }

        UINT32 next_packet_size = 0;
        hr = pCaptureClient->GetNextPacketSize(&next_packet_size);
        exit_on_failed(hr);

        if (next_packet_size == 0) {
            continue;
        }

        BYTE* pData{};
        UINT32 numFramesAvailable{};
        DWORD dwFlags{};

        hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &dwFlags, NULL, NULL);
        exit_on_failed(hr);

        int frame_bytes = pDeviceFormat->nChannels * pDeviceFormat->wBitsPerSample / 8;
        int count = numFramesAvailable * frame_bytes;

        this->broadcast_audio_data((const char*)pData, count);

        data_ckSize += count;
        frame_count += numFramesAvailable;
        seconds = frame_count / pDeviceFormat->nSamplesPerSec;
        //std::cout << "numFramesAvailable: " << numFramesAvailable << " seconds: " << seconds << std::endl;

        hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
        exit_on_failed(hr);

    } while (true);
}

void printEndpoints(CComPtr<IMMDeviceCollection> pColletion)
{
    HRESULT hr{};

    UINT count{};
    hr = pColletion->GetCount(&count);
    exit_on_failed(hr);

    for (UINT i = 0; i < count; ++i) {
        CComPtr<IMMDevice> pEndpoint;
        hr = pColletion->Item(i, &pEndpoint);
        exit_on_failed(hr);

        CComHeapPtr<WCHAR> pwszID;
        hr = pEndpoint->GetId(&pwszID);
        exit_on_failed(hr);

        CComPtr<IPropertyStore> pProps;
        hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        exit_on_failed(hr);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        exit_on_failed(hr);

        std::cout << wchars_to_mbs(varName.pwszVal) << std::endl;

        PropVariantClear(&varName);
    }
}

constexpr inline void exit_on_failed(HRESULT hr) {
    if (FAILED(hr)) {
        exit(-1);
    }
}

void audio_manager::add_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer)
{
    _playing_peer_list.insert(peer);
    std::cout << "add " << peer->remote_endpoint() << std::endl;
}

void audio_manager::remove_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer)
{
    _playing_peer_list.erase(peer);
    std::cout << "remove " << peer->remote_endpoint() << std::endl;
}

const std::vector<uint8_t>& audio_manager::get_format() const
{
    return _format;
}

std::map<std::wstring, std::wstring> audio_manager::get_audio_endpoint_map()
{
    HRESULT hr{};

    CComPtr<IMMDeviceEnumerator> pEnumerator;
    hr = pEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exit_on_failed(hr);

    CComPtr<IMMDeviceCollection> pColletion;
    hr = pEnumerator->EnumAudioEndpoints(eRender, _endpoint_state_mask, &pColletion);
    exit_on_failed(hr);

    UINT count{};
    hr = pColletion->GetCount(&count);
    exit_on_failed(hr);

    std::map<std::wstring, std::wstring> endpoint_map;

    for (UINT i = 0; i < count; ++i) {
        CComPtr<IMMDevice> pEndpoint;
        hr = pColletion->Item(i, &pEndpoint);
        exit_on_failed(hr);

        CComHeapPtr<WCHAR> pwszID;
        hr = pEndpoint->GetId(&pwszID);
        exit_on_failed(hr);

        CComPtr<IPropertyStore> pProps;
        hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        exit_on_failed(hr);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        exit_on_failed(hr);

        endpoint_map[(LPWSTR)pwszID] = varName.pwszVal;

        PropVariantClear(&varName);
    }

    return endpoint_map;
}

std::wstring audio_manager::get_default_endpoint()
{
    HRESULT hr{};

    CComPtr<IMMDeviceEnumerator> pEnumerator;
    hr = pEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exit_on_failed(hr);

    CComPtr<IMMDevice> pEndpoint;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
    exit_on_failed(hr);

    CComHeapPtr<WCHAR> pwszID;
    hr = pEndpoint->GetId(&pwszID);
    exit_on_failed(hr);

    return (LPWSTR)pwszID;
}

void audio_manager::set_format(WAVEFORMATEX* format)
{
    auto f = std::make_unique<AudioFormat>();
    {
        if (format->wFormatTag == WAVE_FORMAT_PCM || format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            f->set_format_tag(format->wFormatTag);
        }
        else if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            auto p = (WAVEFORMATEXTENSIBLE*)(format);
            if (IsEqualGUID(p->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
                f->set_format_tag(WAVE_FORMAT_PCM);
            }
            else if (IsEqualGUID(p->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
                f->set_format_tag(WAVE_FORMAT_IEEE_FLOAT);
            }
        }
    }
    f->set_channels(format->nChannels);
    f->set_sample_rate(format->nSamplesPerSec);
    f->set_bits_per_sample(format->wBitsPerSample);
    _format.resize(f->ByteSizeLong());
    f->SerializeToArray(_format.data(), (int)_format.size());
}

void audio_manager::broadcast_audio_data(const char* data, size_t count)
{
    uint32_t size = (uint32_t)count;
    if (size <= 0) {
        return;
    }

    uint32_t cmd = (uint32_t)cmd_t::cmd_start_play;

    auto p = std::make_shared<std::vector<uint8_t>>(4 + 4 + count);
    std::copy((const uint8_t*)&cmd, (const uint8_t*)&cmd + 4, p->data());
    std::copy((const uint8_t*)&size, (const uint8_t*)&size + 4, p->data() + 4);
    std::copy((const uint8_t*)data, (const uint8_t*)data + count, p->data() + 4 + 4);
    for (auto& peer : _playing_peer_list) {
        asio::async_write(*peer, asio::buffer(*p), [p](const asio::error_code& ec, std::size_t bytes_transferred) {});
    }
}
