/*
   Copyright 2022-2024 mkckr0 <https://github.com/mkckr0>

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

#include <spdlog/spdlog.h>
#include <wil/com.h>
#include <iostream>
#include <vector>
#include <cstdlib>

#include <initguid.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <Audiopolicy.h>

using namespace io::github::mkckr0::audio_share_app::pb;

std::string to_string(PWAVEFORMATEX pFormat)
{
    std::ostringstream ss;
    ss << "\twFormatTag: ";
    switch (pFormat->wFormatTag) {
    case WAVE_FORMAT_PCM:
        ss << "WAVE_FORMAT_PCM";
        break;
    case WAVE_FORMAT_IEEE_FLOAT:
        ss << "WAVE_FORMAT_IEEE_FLOAT";
        break;
    case WAVE_FORMAT_EXTENSIBLE:
        ss << "WAVE_FORMAT_EXTENSIBLE";
        break;
    default:
        ss << pFormat->wFormatTag;
        break;
    }
    ss << "\n"
       << "\tnChannels: " << pFormat->nChannels << "\n"
       << "\tnSamplesPerSec: " << pFormat->nSamplesPerSec << "\n"
       << "\tnAvgBytesPerSec: " << pFormat->nAvgBytesPerSec << "\n"
       << "\tnBlockAlign: " << pFormat->nBlockAlign << "\n"
       << "\twBitsPerSample: " << pFormat->wBitsPerSample << "\n"
       << "\tcbSize: " << pFormat->cbSize << "\n";
    if (pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto pFormatExt = (PWAVEFORMATEXTENSIBLE)pFormat;
        ss << "\tSamples.wValidBitsPerSample: " << pFormatExt->Samples.wValidBitsPerSample << "\n"
           << "\tSamples.wSamplesPerBlock: " << pFormatExt->Samples.wSamplesPerBlock << "\n"
           << "\tdwChannelMask: " << pFormatExt->dwChannelMask << "\n"
           << "\tSubFormat: ";
        if (pFormatExt->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            ss << "KSDATAFORMAT_SUBTYPE_PCM" << "\n";
        } else if (pFormatExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            ss << "KSDATAFORMAT_SUBTYPE_IEEE_FLOAT" << "\n";
        } else {
            WCHAR lpsz[sizeof(GUID)];
            StringFromGUID2(pFormatExt->SubFormat, lpsz, sizeof(GUID));
            ss << wchars_to_mbs(lpsz) << "\n";
        }
    }
    return ss.str();
}

std::wstring to_wstring(PWAVEFORMATEX pFormat)
{
    return mbs_to_wchars(to_string(pFormat));
}

template <>
struct fmt::formatter<WAVEFORMATEX> : fmt::formatter<std::string_view> {
    auto format(WAVEFORMATEX& wave_format, format_context& ctx) const
    {
        return formatter<std::string_view>::format(to_string(&wave_format), ctx);
    }
};

static void exit_on_failed(HRESULT hr, const char* message = "", const char* func = "");
static void print_endpoints(wil::com_ptr<IMMDeviceCollection>& pCollection);
static void set_format(std::shared_ptr<AudioFormat>& _format, PWAVEFORMATEX pFormat);

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

void audio_manager::do_loopback_recording(std::shared_ptr<network_manager> network_manager, const capture_config& config)
{
    spdlog::info("endpoint_id: {}", config.endpoint_id);

    HRESULT hr;

    auto pEnumerator = wil::CoCreateInstance<MMDeviceEnumerator, IMMDeviceEnumerator>();

    wil::com_ptr<IMMDevice> pEndpoint;
    if (config.endpoint_id.empty() || config.endpoint_id == "default") {
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
        exit_on_failed(hr, "can't find default audio endpoint");
    } else {
        hr = pEnumerator->GetDevice(mbs_to_wchars(config.endpoint_id).c_str(), &pEndpoint);
        exit_on_failed(hr);
    }

    wil::com_ptr<IPropertyStore> pProps;
    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
    exit_on_failed(hr);
    PROPVARIANT varName;
    PropVariantInit(&varName);
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    exit_on_failed(hr);
    spdlog::info("select audio endpoint: {}", wchars_to_mbs(varName.pwszVal));
    PropVariantClear(&varName);

    wil::com_ptr<IAudioClient> pAudioClient;
    hr = pEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    exit_on_failed(hr);

    wil::unique_cotaskmem_ptr<WAVEFORMATEX> pMixFormat;
    pAudioClient->GetMixFormat(wil::out_param(pMixFormat));
    spdlog::info("default mix format:\n{}", *pMixFormat);

    wil::unique_cotaskmem_ptr<WAVEFORMATEX> pCaptureFormat;
    if (pMixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        pCaptureFormat.reset((PWAVEFORMATEX)CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
        std::memcpy(pCaptureFormat.get(), pMixFormat.get(), sizeof(WAVEFORMATEXTENSIBLE));
    } else {
        pCaptureFormat = wil::make_unique_cotaskmem<WAVEFORMATEX>(*pMixFormat);
    }
    
    if (config.encoding == encoding_t::encoding_invalid) {
        spdlog::error("invalid encoding");
        return;
    } else if (config.encoding != encoding_t::encoding_default) {
        if (pCaptureFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            ((PWAVEFORMATEXTENSIBLE)pCaptureFormat.get())->SubFormat = config.encoding == encoding_t::encoding_f32 ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
        } else {
            pCaptureFormat->wFormatTag = config.encoding == encoding_t::encoding_f32 ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
        }
        switch (config.encoding) {
        case encoding_t::encoding_f32:
            pCaptureFormat->wBitsPerSample = 32;
            break;
        case encoding_t::encoding_s8:
            pCaptureFormat->wBitsPerSample = 8;
            break;
        case encoding_t::encoding_s16:
            pCaptureFormat->wBitsPerSample = 16;
            break;
        case encoding_t::encoding_s24:
            pCaptureFormat->wBitsPerSample = 24;
            break;
        case encoding_t::encoding_s32:
            pCaptureFormat->wBitsPerSample = 32;
            break;
        default:
            break;
        }
        if (pCaptureFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            ((PWAVEFORMATEXTENSIBLE)pCaptureFormat.get())->Samples.wValidBitsPerSample = pCaptureFormat->wBitsPerSample;
        }
    }
    if (config.channels) {
        pCaptureFormat->nChannels = config.channels;
    }
    pCaptureFormat->nBlockAlign = pCaptureFormat->wBitsPerSample * pCaptureFormat->nChannels / 8;
    if (config.sample_rate) {
        pCaptureFormat->nSamplesPerSec = config.sample_rate;
    }
    pCaptureFormat->nAvgBytesPerSec = pCaptureFormat->nSamplesPerSec * pCaptureFormat->nBlockAlign;

    spdlog::info("capture format:\n{}", *pCaptureFormat);

    // check format is valid
    wil::unique_cotaskmem_ptr<WAVEFORMATEX> pClosestMatchFormat;
    hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pCaptureFormat.get(), wil::out_param(pClosestMatchFormat));
    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
        spdlog::error("the capture format is not supported");
        return;
    }
    else if (hr == S_FALSE) {
        spdlog::warn("the specified capture format is not supported, using a similar format");
        pCaptureFormat = std::move(pClosestMatchFormat);
    }
    else if (hr == S_OK) {
        spdlog::info("the specified capture format is supported");
    }
    else {
        exit_on_failed(hr);
    }

    set_format(_format, pCaptureFormat.get());

    constexpr int REFTIMES_PER_SEC = 10000000; // 1 reference_time = 100ns
    constexpr int REFTIMES_PER_MILLISEC = 10000;

    REFERENCE_TIME hnsMinimumDevicePeriod = 0;
    hr = pAudioClient->GetDevicePeriod(nullptr, &hnsMinimumDevicePeriod);
    exit_on_failed(hr);

    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC; // 1s buffer
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pCaptureFormat.get(), nullptr);
    exit_on_failed(hr);

    UINT32 bufferFrameCount {};
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    exit_on_failed(hr);

    spdlog::info("buffer size: {}", bufferFrameCount);

    wil::com_ptr<IAudioCaptureClient> pCaptureClient;
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    exit_on_failed(hr);

    hr = pAudioClient->Start();
    exit_on_failed(hr);

    const std::chrono::milliseconds duration { hnsMinimumDevicePeriod / REFTIMES_PER_MILLISEC };
    spdlog::info("device period: {}ms", duration.count());

#ifdef DEBUG
    UINT32 frame_count = 0;
    int seconds {};
#endif

    using namespace std::chrono_literals;
    asio::steady_timer timer(*network_manager->_ioc);
    std::error_code ec;

    timer.expires_at(std::chrono::steady_clock::now());

    do {
        timer.expires_at(timer.expiry() + duration);
        timer.wait(ec);
        if (ec) {
            break;
        }

        UINT32 next_packet_size = 0;
        hr = pCaptureClient->GetNextPacketSize(&next_packet_size);
        exit_on_failed(hr, "pCaptureClient->GetNextPacketSize");

        if (next_packet_size == 0) {
            continue;
        }

        BYTE* pData {};
        UINT32 numFramesAvailable {};
        DWORD dwFlags {};

        hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &dwFlags, nullptr, nullptr);
        exit_on_failed(hr, "pCaptureClient->GetBuffer");

        int bytes_per_frame = pCaptureFormat->nBlockAlign;
        size_t count = numFramesAvailable * bytes_per_frame;

        network_manager->broadcast_audio_data((const char*)pData, count, pCaptureFormat->nBlockAlign);

#ifdef DEBUG
        frame_count += numFramesAvailable;
        seconds = frame_count / pCaptureFormat->nSamplesPerSec;
        // spdlog::trace("numFramesAvailable: {}, seconds: {}", numFramesAvailable, seconds);
#endif // DEBUG

        hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
        exit_on_failed(hr, "pCaptureClient->ReleaseBuffer");

    } while (!_stopped);
}

audio_manager::endpoint_list_t audio_manager::get_endpoint_list()
{
    HRESULT hr {};

    auto pEnumerator = wil::CoCreateInstance<MMDeviceEnumerator, IMMDeviceEnumerator>();

    wil::com_ptr<IMMDeviceCollection> pCollection;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    exit_on_failed(hr);

//    print_endpoints(pCollection);

    UINT count {};
    hr = pCollection->GetCount(&count);
    exit_on_failed(hr);

    endpoint_list_t endpoint_list;

    for (UINT i = 0; i < count; ++i) {
        wil::com_ptr<IMMDevice> pEndpoint;
        hr = pCollection->Item(i, &pEndpoint);
        exit_on_failed(hr);

        wil::unique_cotaskmem_ptr<WCHAR> pwszID;
        hr = pEndpoint->GetId(wil::out_param(pwszID));
        exit_on_failed(hr);

        wil::com_ptr<IPropertyStore> pProps;
        hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        exit_on_failed(hr);

        wil::unique_prop_variant varName;
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        exit_on_failed(hr);

        auto endpoint_id = wchars_to_mbs((LPWSTR)pwszID.get());
        endpoint_list.emplace_back(endpoint_id, wchars_to_mbs(varName.pwszVal));
    }

    return endpoint_list;
}

std::string audio_manager::get_default_endpoint()
{
    HRESULT hr {};

    auto pEnumerator = wil::CoCreateInstance<MMDeviceEnumerator, IMMDeviceEnumerator>();

    wil::com_ptr<IMMDevice> pEndpoint;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
    if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
        return {};
    }
    exit_on_failed(hr);

    wil::unique_cotaskmem_ptr<WCHAR> pwszID;
    hr = pEndpoint->GetId(wil::out_param(pwszID));
    exit_on_failed(hr);

    return wchars_to_mbs((LPWSTR)pwszID.get());
}

static void set_format(std::shared_ptr<AudioFormat>& _format, PWAVEFORMATEX pFormat)
{
    auto encoding = AudioFormat_Encoding_ENCODING_INVALID;
    if (pFormat->wFormatTag == WAVE_FORMAT_PCM || pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && PWAVEFORMATEXTENSIBLE(pFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
        switch (pFormat->wBitsPerSample) {
        case 8:
            encoding = AudioFormat_Encoding_ENCODING_PCM_8BIT;
            break;
        case 16:
            encoding = AudioFormat_Encoding_ENCODING_PCM_16BIT;
            break;
        case 24:
            encoding = AudioFormat_Encoding_ENCODING_PCM_24BIT;
            break;
        case 32:
            encoding = AudioFormat_Encoding_ENCODING_PCM_32BIT;
            break;
        }
        _format->set_encoding(encoding);
    }
    if (pFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && PWAVEFORMATEXTENSIBLE(pFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
        encoding = AudioFormat_Encoding_ENCODING_PCM_FLOAT;
    }
    _format->set_encoding(encoding);
    _format->set_channels(pFormat->nChannels);
    _format->set_sample_rate((int32_t)pFormat->nSamplesPerSec);

    spdlog::info("WAVEFORMATEX:\n{}", *pFormat);
    spdlog::info("AudioFormat:\n{}", _format->DebugString());
}

static void print_endpoints(wil::com_ptr<IMMDeviceCollection>& pCollection)
{
    HRESULT hr;

    UINT count {};
    hr = pCollection->GetCount(&count);
    exit_on_failed(hr);

    for (UINT i = 0; i < count; ++i) {

        std::wstringstream ss;

        wil::com_ptr<IMMDevice> pEndpoint;
        hr = pCollection->Item(i, &pEndpoint);
        exit_on_failed(hr);

        wil::unique_cotaskmem_ptr<WCHAR> pwszID;
        hr = pEndpoint->GetId(wil::out_param(pwszID));
        exit_on_failed(hr);
        ss << "Id: " << pwszID.get() << "\n";

        wil::com_ptr<IPropertyStore> pProps;
        hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        exit_on_failed(hr);

        wil::unique_prop_variant varFriendlyName;
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varFriendlyName);
        exit_on_failed(hr);
        ss << "PKEY_Device_FriendlyName: " << varFriendlyName.pwszVal << "\n";

        wil::unique_prop_variant varInstanceId;
        hr = pProps->GetValue(PKEY_Device_InstanceId, &varInstanceId);
        exit_on_failed(hr);
        ss << "PKEY_Device_InstanceId: " << (varInstanceId.vt == VT_LPWSTR ? varInstanceId.pwszVal : L"") << "\n";

        wil::unique_prop_variant varContainerId;
        hr = pProps->GetValue(PKEY_Device_ContainerId, &varContainerId);
        exit_on_failed(hr);
        ss << "PKEY_Device_ContainerId: " << (varContainerId.vt == VT_LPWSTR ? varContainerId.pwszVal : L"") << "\n";

        wil::unique_prop_variant varDeviceDesc;
        hr = pProps->GetValue(PKEY_Device_DeviceDesc, &varDeviceDesc);
        exit_on_failed(hr);
        ss << "PKEY_Device_DeviceDesc: " << varDeviceDesc.pwszVal << "\n";

        wil::unique_prop_variant varDeviceInterfaceName;
        hr = pProps->GetValue(PKEY_DeviceInterface_FriendlyName, &varDeviceInterfaceName);
        exit_on_failed(hr);
        ss << "PKEY_DeviceInterface_FriendlyName: " << varDeviceInterfaceName.pwszVal << "\n";

        wil::unique_prop_variant varGUID;
        hr = pProps->GetValue(PKEY_AudioEndpoint_GUID, &varGUID);
        exit_on_failed(hr);
        ss << "PKEY_AudioEndpoint_GUID: " << varGUID.pwszVal << "\n";

        wil::unique_prop_variant varDeviceFormat;
        hr = pProps->GetValue(PKEY_AudioEngine_DeviceFormat, &varDeviceFormat);
        exit_on_failed(hr);
        ss << "PKEY_AudioEngine_DeviceFormat: \n";
        if (varDeviceFormat.vt == VT_BLOB) {
            auto pFormat = (WAVEFORMATEX*)varDeviceFormat.blob.pBlobData;
            ss << to_wstring(pFormat);
        }

        wil::unique_prop_variant varOemFormat;
        hr = pProps->GetValue(PKEY_AudioEngine_OEMFormat, &varOemFormat);
        exit_on_failed(hr);
        ss << "PKEY_AudioEngine_OEMFormat: \n";
        if (varOemFormat.vt == VT_BLOB) {
            auto pFormat = (WAVEFORMATEX*)varOemFormat.blob.pBlobData;
            ss << to_wstring(pFormat);
        }

        wil::unique_prop_variant varJackSubType;
        hr = pProps->GetValue(PKEY_AudioEndpoint_JackSubType, &varJackSubType);
        exit_on_failed(hr);
        ss << "PKEY_AudioEndpoint_JackSubType: " << varJackSubType.pwszVal << "\n";

        spdlog::info("{}", wchars_to_mbs(ss.str()));
        // spdlog::info("{}", wchars_to_utf8(varFriendlyName.pwszVal));
    }
}

static void exit_on_failed(HRESULT hr, const char* message, const char* func)
{
    if (FAILED(hr)) {
        spdlog::error("exit_on_failed {} {}, {}", func, message, wchars_to_mbs(wstr_win_err(HRESULT_CODE(hr))));
        exit(-1);
    }
}

std::string wchars_to_mbs(const std::wstring& src)
{
    UINT cp = GetACP();
    int n = WideCharToMultiByte(cp, 0, src.data(), (int)src.size(), nullptr, 0, nullptr, nullptr);

    std::vector<char> buf(n);
    WideCharToMultiByte(cp, 0, src.data(), (int)src.size(), buf.data(), (int)buf.size(), nullptr, nullptr);
    std::string dst(buf.data(), buf.size());
    return dst;
}

std::string wchars_to_utf8(const std::wstring& src)
{
    UINT cp = CP_UTF8;
    int n = WideCharToMultiByte(cp, 0, src.data(), (int)src.size(), nullptr, 0, nullptr, nullptr);

    std::vector<char> buf(n);
    WideCharToMultiByte(cp, 0, src.data(), (int)src.size(), buf.data(), (int)buf.size(), nullptr, nullptr);
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