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

#ifdef _WINDOWS

#include "audio_manager.hpp"
#include "client.pb.h"
#include "network_manager.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <vector>

#include <atlbase.h>
#include <combaseapi.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <Audiopolicy.h>

#include <spdlog/spdlog.h>

using namespace io::github::mkckr0::audio_share_app::pb;

constexpr inline void exit_on_failed(HRESULT hr);
void print_endpoints(CComPtr<IMMDeviceCollection> pColletion);
void set_format(std::shared_ptr<AudioFormat> _format, WAVEFORMATEX* format);

namespace detail {

audio_manager_impl::audio_manager_impl()
{
    (void)CoInitializeEx(nullptr, COINIT::COINIT_MULTITHREADED);
}

audio_manager_impl::~audio_manager_impl()
{
    CoUninitialize();
}

} // namespace detail

void audio_manager::do_loopback_recording(std::shared_ptr<network_manager> network_manager, const std::string& endpoint_id)
{
    spdlog::info("endpoint_id: {}", endpoint_id);

    HRESULT hr {};

    CComPtr<IMMDeviceEnumerator> pEnumerator;
    hr = pEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exit_on_failed(hr);

    CComPtr<IMMDevice> pEndpoint;
    hr = pEnumerator->GetDevice(mbs_to_wchars(endpoint_id).c_str(), &pEndpoint);
    exit_on_failed(hr);

    CComPtr<IPropertyStore> pProps;
    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
    exit_on_failed(hr);
    PROPVARIANT varName;
    PropVariantInit(&varName);
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    exit_on_failed(hr);
    spdlog::info("select audio endpoint: {}", wchars_to_mbs(varName.pwszVal));
    PropVariantClear(&varName);

    CComPtr<IAudioClient> pAudioClient;
    hr = pEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    exit_on_failed(hr);

    CComHeapPtr<WAVEFORMATEX> pCaptureFormat;
    pAudioClient->GetMixFormat(&pCaptureFormat);

    set_format(_format, pCaptureFormat);

    constexpr int REFTIMES_PER_SEC = 10000000; // 1 reference_time = 100ns
    constexpr int REFTIMES_PER_MILLISEC = 10000;

    const REFERENCE_TIME hnsRequestedDuration = 1 * REFTIMES_PER_SEC; // 1s
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pCaptureFormat, nullptr);
    exit_on_failed(hr);

    UINT32 bufferFrameCount {};
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

    int seconds {};

    using namespace std::chrono_literals;
    asio::steady_timer timer(*network_manager->_ioc);
    std::error_code ec;

    do {
        timer.expires_after(1ms);
        timer.wait(ec);
        if (ec) {
            break;
        }

        UINT32 next_packet_size = 0;
        hr = pCaptureClient->GetNextPacketSize(&next_packet_size);
        exit_on_failed(hr);

        if (next_packet_size == 0) {
            continue;
        }

        BYTE* pData {};
        UINT32 numFramesAvailable {};
        DWORD dwFlags {};

        hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &dwFlags, NULL, NULL);
        exit_on_failed(hr);

        int bytes_per_frame = pCaptureFormat->nBlockAlign;
        int count = numFramesAvailable * bytes_per_frame;

        network_manager->broadcast_audio_data((const char*)pData, count, pCaptureFormat->nBlockAlign);

        data_ckSize += count;
        frame_count += numFramesAvailable;
        seconds = frame_count / pCaptureFormat->nSamplesPerSec;
        // std::cout << "numFramesAvailable: " << numFramesAvailable << " seconds: " << seconds << std::endl;

        hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
        exit_on_failed(hr);

    } while (!_stoppped);
}

int audio_manager::get_endpoint_list(endpoint_list_t& endpoint_list)
{
    HRESULT hr {};

    CComPtr<IMMDeviceEnumerator> pEnumerator;
    hr = pEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exit_on_failed(hr);

    CComPtr<IMMDeviceCollection> pColletion;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pColletion);
    exit_on_failed(hr);

    UINT count {};
    hr = pColletion->GetCount(&count);
    exit_on_failed(hr);

    int default_index = -1;
    std::string default_id = get_default_endpoint();

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

        auto endpoint_id = wchars_to_mbs((LPWSTR)pwszID);
        endpoint_list.push_back(std::make_pair(endpoint_id, wchars_to_mbs(varName.pwszVal)));

        if (endpoint_id == default_id) {
            default_index = i;
        }

        PropVariantClear(&varName);
    }

    return default_index;
}

std::string audio_manager::get_default_endpoint()
{
    HRESULT hr {};

    CComPtr<IMMDeviceEnumerator> pEnumerator;
    hr = pEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    exit_on_failed(hr);

    CComPtr<IMMDevice> pEndpoint;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
    exit_on_failed(hr);

    CComHeapPtr<WCHAR> pwszID;
    hr = pEndpoint->GetId(&pwszID);
    exit_on_failed(hr);

    return wchars_to_mbs((LPWSTR)pwszID);
}

void set_format(std::shared_ptr<AudioFormat> _format, WAVEFORMATEX* format)
{
    if (format->wFormatTag == WAVE_FORMAT_PCM || format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        _format->set_format_tag(format->wFormatTag);
    } else if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto p = (WAVEFORMATEXTENSIBLE*)(format);
        if (IsEqualGUID(p->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
            _format->set_format_tag(WAVE_FORMAT_PCM);
        } else if (IsEqualGUID(p->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            _format->set_format_tag(WAVE_FORMAT_IEEE_FLOAT);
        }
    }
    _format->set_channels(format->nChannels);
    _format->set_sample_rate(format->nSamplesPerSec);
    _format->set_bits_per_sample(format->wBitsPerSample);

    spdlog::info("WAVEFORMATEX: wFormatTag: {}, nBlockAlign: {}", format->wFormatTag, format->nBlockAlign);
    spdlog::info("AudioFormat:\n{}", _format->DebugString());
}

void print_endpoints(CComPtr<IMMDeviceCollection> pColletion)
{
    HRESULT hr {};

    UINT count {};
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

        spdlog::info("{}", wchars_to_mbs(varName.pwszVal));

        PropVariantClear(&varName);
    }
}

constexpr inline void exit_on_failed(HRESULT hr)
{
    if (FAILED(hr)) {
        exit(-1);
    }
}

std::string wchars_to_mbs(const std::wstring& src)
{
    UINT cp = GetACP();
    int n = WideCharToMultiByte(cp, 0, src.data(), (int)src.size(), nullptr, 0, 0, 0);

    std::vector<char> buf(n);
    WideCharToMultiByte(cp, 0, src.data(), (int)src.size(), buf.data(), (int)buf.size(), 0, 0);
    std::string dst(buf.data(), buf.size());
    return dst;
}

std::wstring mbs_to_wchars(const std::string& src)
{
    UINT cp = GetACP();
    int n = MultiByteToWideChar(cp, 0, src.data(), (int)src.size(), nullptr, 0);

    std::vector<wchar_t> buf(n);
    MultiByteToWideChar(cp, 0, src.data(), (int)src.size(), buf.data(), (int)buf.size());
    std::wstring dst(buf.data(), buf.size());
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
        nullptr);
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
        nullptr);
    std::wstring msg;
    if (buf) {
        msg.assign(buf);
        LocalFree(buf);
    }
    return msg;
}

#endif // _WINDOWS