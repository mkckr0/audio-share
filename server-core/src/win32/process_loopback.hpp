#pragma once

#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wil/com.h>
#include <wrl.h>

#pragma comment(lib, "Mmdevapi.lib")

namespace detail {

// The completion runs on an MTA thread. The event also publishes the result
// before the capture thread queries the activated interface.
class loopback_activation final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IActivateAudioInterfaceCompletionHandler, Microsoft::WRL::FtmBase> {
public:
    wil::unique_event_nothrow completed;
    AUDIOCLIENT_ACTIVATION_PARAMS options{};
    PROPVARIANT params{};

    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation*) override
    {
        SetEvent(completed.get());
        return S_OK;
    }
};

inline HRESULT activate_without_microphone(DWORD pid, IAudioClient** client)
{
    auto completion = Microsoft::WRL::Make<loopback_activation>();
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, completion.Get());
    RETURN_IF_FAILED(completion->completed.create());

    auto& options = completion->options;
    options.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    options.ProcessLoopbackParams.TargetProcessId = pid;
    options.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE;
    auto& params = completion->params;
    params.vt = VT_BLOB;
    params.blob.cbSize = sizeof(options);
    params.blob.pBlobData = reinterpret_cast<BYTE*>(&options);
    wil::com_ptr_nothrow<IActivateAudioInterfaceAsyncOperation> operation;
    RETURN_IF_FAILED(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                                &params, completion.Get(), &operation));
    // AudioSrv can disappear during activation. The callback owns the
    // parameters and event even if completion arrives after this timeout.
    if (!completion->completed.wait(3000)) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    HRESULT result = E_FAIL;
    wil::com_ptr_nothrow<IUnknown> activated;
    RETURN_IF_FAILED(operation->GetActivateResult(&result, &activated));
    RETURN_IF_FAILED(result);
    return activated->QueryInterface(IID_PPV_ARGS(client));
}

} // namespace detail
