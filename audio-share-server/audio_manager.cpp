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

int audio_manager::add_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer)
{
    if (_playing_peer_list.contains(peer)) {
        spdlog::error("{} repeat add tcp://{}", __func__, peer->remote_endpoint());
        return 0;
    }

    auto info = _playing_peer_list[peer] = std::make_shared<peer_info_t>();
    static int g_id = 0;
    info->id = ++g_id;
    info->tcp_peer = peer;

    spdlog::info("{} add id:{} tcp://{}", __func__, info->id, peer->remote_endpoint());
    return info->id;
}

void audio_manager::remove_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer)
{
    if (!_playing_peer_list.contains(peer)) {
        spdlog::error("{} repeat remove tcp://{}", __func__, peer->remote_endpoint());
        return;
    }

    _playing_peer_list.erase(peer);
    spdlog::info("{} remove tcp://{}", __func__, peer->remote_endpoint());
}

void audio_manager::fill_udp_peer(int id, asio::ip::udp::endpoint udp_peer)
{
    auto it = std::find_if(_playing_peer_list.begin(), _playing_peer_list.end(), [id](const std::pair<std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<peer_info_t>>& e) {
        return e.second->id == id;
        });

    if (it == _playing_peer_list.cend()) {
        spdlog::error("{} no tcp peer id:{} udp://{}", __func__, id, udp_peer);
        return;
    }

    it->second->udp_peer = udp_peer;
    spdlog::info("{} fill udp peer id:{} tcp://{} udp://{}", __func__, id, it->second->tcp_peer->remote_endpoint(), udp_peer);
}

void audio_manager::init_udp_server(std::shared_ptr<asio::ip::udp::socket> udp_server)
{
    _udp_server = udp_server;
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

    //spdlog::info("size: {}", count);

    // divide udp frame
    constexpr int mtu = 1492, seg_size = mtu - 20 - 8;
    int i = 0;
    std::list<std::shared_ptr<std::vector<uint8_t>>> seg_list;
    for (; i < count; i += seg_size) {
        const int real_seg_size = std::min((int)count - i, seg_size);
        auto p = std::make_shared<std::vector<uint8_t>>(real_seg_size);
        std::copy((const uint8_t*)data + i, (const uint8_t*)data + i + real_seg_size, p->data());
        seg_list.push_back(p);
    }

    for (auto seg : seg_list) {
        for (auto& [peer, info] : _playing_peer_list) {
            _udp_server->async_send_to(asio::buffer(*seg), info->udp_peer, [seg](const asio::error_code& ec, std::size_t bytes_transferred) {});
        }
    }
}
