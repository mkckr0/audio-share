#include "wasapi_capture.h"
#include <chrono>
#include <iostream>
#include <cmath>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#endif

namespace audiostream::audio {

WasapiAudioCapture::WasapiAudioCapture() {
    format_.sample_rate = 48000;
    format_.channels = 2;
    format_.bits_per_sample = 16;
    format_.is_float = false;
}

WasapiAudioCapture::~WasapiAudioCapture() {
    stop();
}

std::vector<std::pair<std::string, std::string>> WasapiAudioCapture::list_render_devices() {
    std::vector<std::pair<std::string, std::string>> devices;
    devices.push_back({"default", "Default Audio Endpoint (Loopback)"});
    return devices;
}

bool WasapiAudioCapture::start(AudioDataCallback callback, const std::string& device_id) {
    if (is_capturing_) stop();
    data_callback_ = callback;
    device_id_ = device_id;
    is_capturing_ = true;
    capture_thread_ = std::thread(&WasapiAudioCapture::capture_thread_func, this);
    return true;
}

void WasapiAudioCapture::stop() {
    is_capturing_ = false;
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
}

void WasapiAudioCapture::capture_thread_func() {
#if defined(_WIN32) && !defined(AUDIOSTREAM_HEADLESS_SIM)
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audio_client = nullptr;
    IAudioCaptureClient* capture_client = nullptr;
    WAVEFORMATEX* pwfx = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (SUCCEEDED(hr)) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    if (SUCCEEDED(hr)) {
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client);
    }
    if (SUCCEEDED(hr)) {
        hr = audio_client->GetMixFormat(&pwfx);
    }
    if (SUCCEEDED(hr)) {
        format_.sample_rate = pwfx->nSamplesPerSec;
        format_.channels = pwfx->nChannels;
        format_.bits_per_sample = pwfx->wBitsPerSample;
        format_.is_float = (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

        REFERENCE_TIME hnsRequestedDuration = 200000; // 20ms
        hr = audio_client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            hnsRequestedDuration,
            0,
            pwfx,
            nullptr
        );
    }
    if (SUCCEEDED(hr)) {
        hr = audio_client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture_client);
    }
    if (SUCCEEDED(hr)) {
        audio_client->Start();
    }

    while (is_capturing_) {
        UINT32 packet_length = 0;
        if (capture_client && SUCCEEDED(capture_client->GetNextPacketSize(&packet_length)) && packet_length > 0) {
            BYTE* data = nullptr;
            UINT32 num_frames_read = 0;
            DWORD flags = 0;
            if (SUCCEEDED(capture_client->GetBuffer(&data, &num_frames_read, &flags, nullptr, nullptr))) {
                if (data_callback_ && num_frames_read > 0) {
                    size_t bytes = num_frames_read * pwfx->nBlockAlign;
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        std::vector<uint8_t> silent(bytes, 0);
                        data_callback_(silent.data(), bytes, format_);
                    } else {
                        data_callback_(data, bytes, format_);
                    }
                }
                capture_client->ReleaseBuffer(num_frames_read);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    if (audio_client) {
        audio_client->Stop();
        audio_client->Release();
    }
    if (capture_client) capture_client->Release();
    if (pwfx) CoTaskMemFree(pwfx);
    if (device) device->Release();
    if (enumerator) enumerator->Release();
    CoUninitialize();
#else
    // Simulated loopback generator: 48kHz stereo 16-bit PCM silence/tone frames every 20ms
    const int frame_samples = 960; // 20ms at 48kHz
    const size_t frame_bytes = frame_samples * 2 * sizeof(int16_t);
    std::vector<int16_t> sample_buf(frame_samples * 2, 0);

    while (is_capturing_) {
        auto start = std::chrono::steady_clock::now();
        if (data_callback_) {
            data_callback_(reinterpret_cast<const uint8_t*>(sample_buf.data()), frame_bytes, format_);
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleep_dur = std::chrono::milliseconds(20) - elapsed;
        if (sleep_dur > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_dur);
        }
    }
#endif
}

} // namespace audiostream::audio
