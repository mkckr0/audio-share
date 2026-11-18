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
#include <functional>
#include <optional>

#include <initguid.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include "win32/microphone_route.hpp"
#include "win32/microphone_endpoint.hpp"
#include "win32/process_loopback.hpp"

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
static std::string get_device_name(IPropertyStore* pProp);

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

// Capture from the selected endpoint or, with explicit consent, the system mix
// excluding the microphone renderer. Both use the same negotiated wire format.
static HRESULT capture_packets(IMMDevice* endpoint, DWORD exclude_pid, WAVEFORMATEX* format, DWORD poll_ms,
                               const std::function<bool()>& current, network_manager& network)
{
    wil::com_ptr_nothrow<IAudioClient> client;
    wil::unique_event_nothrow ready;
    DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK;
    REFERENCE_TIME buffer_duration = 10000000; // one second for legacy endpoint polling
    if (exclude_pid) {
        RETURN_IF_FAILED(detail::activate_without_microphone(exclude_pid, &client));
        RETURN_IF_FAILED(ready.create());
        flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
        buffer_duration = 0;
    } else {
        RETURN_IF_FAILED(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, client.put_void()));
    }
    RETURN_IF_FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, buffer_duration, 0, format, nullptr));
    if (ready) RETURN_IF_FAILED(client->SetEventHandle(ready.get()));
    wil::com_ptr_nothrow<IAudioCaptureClient> capture;
    RETURN_IF_FAILED(client->GetService(IID_PPV_ARGS(&capture)));
    RETURN_IF_FAILED(client->Start());

    std::vector<BYTE> silence;
    while (current()) {
        if (ready) ready.wait(50);
        else std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
        while (current()) {
            UINT32 frames = 0;
            RETURN_IF_FAILED(capture->GetNextPacketSize(&frames));
            if (!frames) break;
            BYTE* data = nullptr;
            DWORD packet_flags = 0;
            const HRESULT result = capture->GetBuffer(&data, &frames, &packet_flags, nullptr, nullptr);
            if (result == AUDCLNT_S_BUFFER_EMPTY) break;
            RETURN_IF_FAILED(result);
            const size_t bytes = static_cast<size_t>(frames) * format->nBlockAlign;
            if (packet_flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                silence.resize(bytes, format->wBitsPerSample == 8 ? 128 : 0);
                data = silence.data();
            }
            // The receiver publishes before rendering. Drop buffered audio if
            // the route changed while WASAPI was providing this packet.
            if (current() && bytes)
                network.broadcast_audio_data(reinterpret_cast<const char*>(data), bytes, format->nBlockAlign);
            RETURN_IF_FAILED(capture->ReleaseBuffer(frames));
        }
    }
    return client->Stop();
}

void audio_manager::do_loopback_recording(std::shared_ptr<network_manager> network_manager, const capture_config& config) try
{
    spdlog::info("endpoint_id: {}", config.endpoint_id);

    HRESULT hr;

    auto com = wil::CoInitializeEx(COINIT_MULTITHREADED);
    auto pEnumerator = wil::CoCreateInstance<MMDeviceEnumerator, IMMDeviceEnumerator>();

    wil::com_ptr<IMMDevice> pEndpoint;
    const bool duplex_default = config.endpoint_id == "duplex";
    if (config.endpoint_id.empty() || config.endpoint_id == "default" || duplex_default) {
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
        exit_on_failed(hr, "can't find default audio endpoint");
    } else {
        hr = pEnumerator->GetDevice(mbs_to_wchars(config.endpoint_id).c_str(), &pEndpoint);
        exit_on_failed(hr);
    }

    wil::com_ptr<IPropertyStore> pProps;
    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
    exit_on_failed(hr);
    auto device_name = get_device_name(pProps.get());
    spdlog::info("select audio endpoint: {}", device_name);
    wil::unique_cotaskmem_string endpoint_id;
    exit_on_failed(pEndpoint->GetId(wil::out_param(endpoint_id)));
    microphone_route route(wchars_to_utf8(endpoint_id.get()));

    wil::com_ptr<IAudioClient> pAudioClient;
    hr = pEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    exit_on_failed(hr);

    wil::unique_cotaskmem_ptr<WAVEFORMATEX> pMixFormat;
    exit_on_failed(pAudioClient->GetMixFormat(wil::out_param(pMixFormat)));
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

    spdlog::info("request capture format:\n{}", *pCaptureFormat);

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

    REFERENCE_TIME period = 0;
    exit_on_failed(pAudioClient->GetDevicePeriod(nullptr, &period));
    const DWORD poll_ms = static_cast<DWORD>((std::max)(1LL, period / 10000));
    const bool allow_system_mix = duplex_default || config.allow_system_mix;
    playback_routing routing(allow_system_mix, detail::can_render_microphone(endpoint_id.get()),
                             playback_routing::clock::now());
    std::optional<uint64_t> watched;
    wil::unique_handle owner;
    auto select_source = [&]() -> playback_source {
        for (;;) {
            const auto epoch = route.snapshot();
            const auto source = routing.next(epoch, playback_routing::clock::now());
            const DWORD pid = static_cast<DWORD>(epoch);
            if (watched != epoch) {
                watched = epoch;
                owner.reset();
                if (pid) {
                    owner.reset(OpenProcess(SYNCHRONIZE, FALSE, pid));
                    if (!allow_system_mix)
                        spdlog::warn("Microphone and speaker share {}. Speaker playback paused. Select a separate playback "
                                     "endpoint or use --allow-system-mix to share all PC outputs except the microphone.", device_name);
                }
            }
            if (!pid) return source;
            const bool exited = owner ? WaitForSingleObject(owner.get(), 0) == WAIT_OBJECT_0 : route.writer_gone();
            if (!exited) return source;
            // A crash leaves the PID behind. Keep its exclusion through the
            // driver's tail and never clear a replacement receiver's epoch.
            route.receiver_exited(epoch);
        }
    };
    using mode = playback_source::mode;
    std::optional<playback_source> failed;
    while (!_stopped) {
        const auto source = select_source();
        auto current = [&] { return !_stopped && select_source() == source; };
        if (source.type == mode::pause) {
            while (current()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (failed != source) {
            if (source.type == mode::exclude_receiver)
                spdlog::info("Sharing system playback excluding microphone receiver PID {}", source.pid);
            else spdlog::info("Sharing playback from {}", device_name);
            failed.reset();
        }
        hr = capture_packets(pEndpoint.get(), source.pid, pCaptureFormat.get(), poll_ms, current, *network_manager);
        if (FAILED(hr)) {
            if (failed != source)
                spdlog::error("Playback capture failed (HRESULT 0x{:08x}). Choose a separate playback endpoint "
                              "if this Windows version cannot exclude the microphone.", static_cast<DWORD>(hr));
            failed = source;
            // Never substitute unfiltered loopback while the mic is rendering.
            for (int i = 0; i < 20 && current(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
} catch (const wil::ResultException& error) {
    spdlog::error("Cannot initialize Windows playback capture: {}", error.what());
}

audio_manager::endpoint_list_t audio_manager::get_endpoint_list()
{
    HRESULT hr {};

    auto pEnumerator = wil::CoCreateInstance<MMDeviceEnumerator, IMMDeviceEnumerator>();

    wil::com_ptr<IMMDeviceCollection> pCollection;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    exit_on_failed(hr, "EnumAudioEndpoints", __func__);

//    print_endpoints(pCollection);

    UINT count {};
    hr = pCollection->GetCount(&count);
    exit_on_failed(hr, "GetCount", __func__);

    endpoint_list_t endpoint_list;

    for (UINT i = 0; i < count; ++i) {
        wil::com_ptr<IMMDevice> pEndpoint;
        hr = pCollection->Item(i, &pEndpoint);
        exit_on_failed(hr, "Item", __func__);

        wil::unique_cotaskmem_ptr<WCHAR> pwszID;
        hr = pEndpoint->GetId(wil::out_param(pwszID));
        exit_on_failed(hr, "GetId", __func__);
        auto endpoint_id = wchars_to_mbs((LPWSTR)pwszID.get());

        wil::com_ptr<IPropertyStore> pProps;
        hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        exit_on_failed(hr, "OpenPropertyStore", __func__);

        auto name = get_device_name(pProps.get());

        endpoint_list.emplace_back(endpoint_id, name);
    }

    if (!endpoint_list.empty()) {
        endpoint_list.emplace_back("duplex", "Default playback (allow all PC outputs when sharing the microphone cable)");
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
    exit_on_failed(hr, "GetDefaultAudioEndpoint", __func__);

    wil::unique_cotaskmem_ptr<WCHAR> pwszID;
    hr = pEndpoint->GetId(wil::out_param(pwszID));
    exit_on_failed(hr, "GetId", __func__);

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

    spdlog::info("result capture format:\n{}", *pFormat);
    spdlog::info("AudioFormat:\n{}", _format->DebugString());
}

static std::string get_device_name(IPropertyStore* pProp)
{
    wil::unique_prop_variant varName;
    pProp->GetValue(PKEY_Device_FriendlyName, &varName);
    std::string name = "[Speakers]";
    if (varName.vt == VT_LPWSTR) {
        name = wchars_to_mbs(varName.pwszVal);
    }
    return name;
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
        ss << "PKEY_Device_FriendlyName: " << (varFriendlyName.vt == VT_LPWSTR ? varFriendlyName.pwszVal : L"") << "\n";

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
        spdlog::error("exit_on_failed hr={}, func={}, message={}, error={}", hr, func, message, str_win_err(HRESULT_CODE(hr)));
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