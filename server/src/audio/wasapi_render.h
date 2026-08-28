#ifndef AUDIOSTREAM_WASAPI_RENDER_H
#define AUDIOSTREAM_WASAPI_RENDER_H

#include <cstdint>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

namespace audiostream::audio {

class WasapiAudioRender {
public:
    WasapiAudioRender();
    ~WasapiAudioRender();

    bool initialize(const std::string& device_name = "CABLE Input", int sample_rate = 48000, int channels = 1);
    void destroy();

    // Render 16-bit PCM buffer into virtual audio device
    bool render_samples(const int16_t* samples, size_t sample_count);

    bool is_active() const { return is_active_; }

private:
    std::atomic<bool> is_active_{false};
    std::string device_name_;
    int sample_rate_ = 48000;
    int channels_ = 1;
    std::mutex render_mutex_;
};

} // namespace audiostream::audio

#endif // AUDIOSTREAM_WASAPI_RENDER_H
