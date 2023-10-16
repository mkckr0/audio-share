/*
   Copyright 2022-2023 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifdef WIN32

#include "audio_manager.hpp"
#include "client.pb.h"
#include "common.hpp"
#include "network_manager.hpp"

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

using namespace io::github::mkckr0::audio_share_app::pb;

std::string wchars_to_mbs(const wchar_t* src);
std::string str_win_err(int err);
std::wstring wstr_win_err(int err);

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

audio_manager::audio_manager(std::shared_ptr<network_manager> network_manager)
    : _network_manager(network_manager)
{
}

audio_manager::~audio_manager()
{
}

asio::awaitable<void> audio_manager::do_loopback_recording(asio::io_context& ioc, const std::wstring& endpoint_id)
{
    HRESULT hr{};

    // CoInitialize has been called by Dialog
    //CoInitializeGuard coInitializeGuard;
    //exit_on_failed(coInitializeGuard.result());

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

    CComHeapPtr<WAVEFORMATEX> pCaptureFormat;
    pAudioClient->GetMixFormat(&pCaptureFormat);

    this->set_format(pCaptureFormat);

    constexpr int REFTIMES_PER_SEC = 10000000;      // 1 reference_time = 100ns
    constexpr int REFTIMES_PER_MILLISEC = 10000;

    const REFERENCE_TIME hnsRequestedDuration = 1 * REFTIMES_PER_SEC; // 1s
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pCaptureFormat, nullptr);
    exit_on_failed(hr);

    UINT32 bufferFrameCount{};
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    exit_on_failed(hr);

    CComPtr<IAudioCaptureClient> pCaptureClient;
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    exit_on_failed(hr);

    hr = pAudioClient->Start();
    exit_on_failed(hr);

    const REFERENCE_TIME hnsActualDuration = (long long)REFTIMES_PER_SEC * bufferFrameCount / pCaptureFormat->nSamplesPerSec;

    UINT32 data_ckSize = 0;
    UINT32 frame_count = 0;

    int seconds{};

    using namespace std::literals;
    steady_timer timer(ioc);
    timer.expires_at(std::chrono::steady_clock::now());

    do {
        timer.expires_at(timer.expiry() + 1ms);
        auto [ec] = co_await timer.async_wait();
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

        int bytes_per_frame = pCaptureFormat->nBlockAlign;
        int count = numFramesAvailable * bytes_per_frame;

        _network_manager->broadcast_audio_data((const char*)pData, count, pCaptureFormat->nBlockAlign);

        data_ckSize += count;
        frame_count += numFramesAvailable;
        seconds = frame_count / pCaptureFormat->nSamplesPerSec;
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
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pColletion);
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
    spdlog::info("WAVEFORMATEX: wFormatTag: {}, nBlockAlign: {}", format->wFormatTag, format->nBlockAlign);
    spdlog::info("AudioFormat:\n{}", f->DebugString());
}

std::string wchars_to_mbs(const wchar_t* src)
{
    UINT cp = GetACP();
    int ccWideChar = (int)wcslen(src);
    int n = WideCharToMultiByte(cp, 0, src, ccWideChar, 0, 0, 0, 0);

    std::vector<char> buf(n);
    WideCharToMultiByte(cp, 0, src, ccWideChar, buf.data(), (int)buf.size(), 0, 0);
    std::string dst(buf.data(), buf.size());
    return dst;
}

std::string str_win_err(int err)
{
	LPSTR buf = nullptr;
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		nullptr,
		err,
		0,
		(LPSTR)&buf,
		0,
		nullptr
	);
	std::string msg;
	if (buf) {
		msg.assign(buf);
		LocalFree(buf);
	}
	return msg;
}

std::wstring wstr_win_err(int err)
{
	LPWSTR buf = nullptr;
	FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		nullptr,
		err,
		0,
		(LPWSTR)&buf,
		0,
		nullptr
	);
	std::wstring msg;
	if (buf) {
		msg.assign(buf);
		LocalFree(buf);
	}
	return msg;
}

#endif // WIN32