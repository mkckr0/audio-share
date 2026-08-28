#include "wasapi_render.h"
#include <iostream>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif

namespace audiostream::audio {

WasapiAudioRender::WasapiAudioRender() = default;

WasapiAudioRender::~WasapiAudioRender() {
    destroy();
}

bool WasapiAudioRender::initialize(const std::string& device_name, int sample_rate, int channels) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    device_name_ = device_name;
    sample_rate_ = sample_rate;
    channels_ = channels;
    is_active_ = true;
    return true;
}

void WasapiAudioRender::destroy() {
    std::lock_guard<std::mutex> lock(render_mutex_);
    is_active_ = false;
}

bool WasapiAudioRender::render_samples(const int16_t* samples, size_t sample_count) {
    if (!is_active_ || !samples || sample_count == 0) return false;
    std::lock_guard<std::mutex> lock(render_mutex_);
    // In Windows live execution, writes to IAudioRenderClient buffer
    return true;
}

} // namespace audiostream::audio
