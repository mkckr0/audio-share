#include "virtual_mic_monitor.h"
#include <iostream>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#endif

namespace audiostream::audio {

VirtualMicMonitor::VirtualMicMonitor(double debounce_seconds)
    : debounce_seconds_(debounce_seconds) {}

VirtualMicMonitor::~VirtualMicMonitor() {
    stop();
}

bool VirtualMicMonitor::start(MicStateChangeCallback callback, const std::string& virtual_device_name) {
    if (is_monitoring_) stop();
    state_callback_ = callback;
    device_name_ = virtual_device_name;
    is_monitoring_ = true;
    current_state_ = MicMonitorState::IDLE;
    active_sessions_count_ = 0;
    monitor_thread_ = std::thread(&VirtualMicMonitor::monitor_thread_func, this);
    return true;
}

void VirtualMicMonitor::stop() {
    is_monitoring_ = false;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void VirtualMicMonitor::set_session_count(int count) {
    active_sessions_count_ = count;
}

int VirtualMicMonitor::query_windows_session_count() {
#if defined(_WIN32) && !defined(AUDIOSTREAM_HEADLESS_SIM)
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    int count = 0;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioSessionManager2* pSessionManager = nullptr;
    IAudioSessionEnumerator* pSessionEnum = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (SUCCEEDED(hr)) {
        hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &pDevice);
    }
    if (SUCCEEDED(hr)) {
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
    }
    if (SUCCEEDED(hr)) {
        hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
    }
    if (SUCCEEDED(hr)) {
        int session_total = 0;
        pSessionEnum->GetCount(&session_total);
        for (int i = 0; i < session_total; ++i) {
            IAudioSessionControl* pSessionControl = nullptr;
            IAudioSessionControl2* pSessionControl2 = nullptr;
            if (SUCCEEDED(pSessionEnum->GetSession(i, &pSessionControl))) {
                if (SUCCEEDED(pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2))) {
                    AudioSessionState state;
                    if (SUCCEEDED(pSessionControl2->GetState(&state)) && state == AudioSessionStateActive) {
                        count++;
                    }
                    pSessionControl2->Release();
                }
                pSessionControl->Release();
            }
        }
        pSessionEnum->Release();
    }
    if (pSessionManager) pSessionManager->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    CoUninitialize();
    return count;
#else
    return active_sessions_count_.load();
#endif
}

void VirtualMicMonitor::monitor_thread_func() {
    while (is_monitoring_) {
        int sessions = query_windows_session_count();
        active_sessions_count_ = sessions;

        auto now = std::chrono::steady_clock::now();

        if (sessions > 0) {
            if (current_state_ != MicMonitorState::ACTIVE) {
                current_state_ = MicMonitorState::ACTIVE;
                if (state_callback_) {
                    state_callback_(true);
                }
            }
        } else {
            // No active sessions
            if (current_state_ == MicMonitorState::ACTIVE) {
                current_state_ = MicMonitorState::DEBOUNCE_WAIT;
                debounce_start_time_ = now;
            } else if (current_state_ == MicMonitorState::DEBOUNCE_WAIT) {
                auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - debounce_start_time_).count();
                if (elapsed >= debounce_seconds_) {
                    current_state_ = MicMonitorState::IDLE;
                    if (state_callback_) {
                        state_callback_(false);
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

} // namespace audiostream::audio
