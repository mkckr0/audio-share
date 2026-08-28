#ifndef AUDIOSTREAM_WASAPI_CAPTURE_H
#define AUDIOSTREAM_WASAPI_CAPTURE_H

#include <cstdint>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <string>

namespace audiostream::audio {

struct AudioFormatDesc {
    int sample_rate = 48000;
    int channels = 2;
    int bits_per_sample = 16;
    bool is_float = false;
};

using AudioDataCallback = std::function<void(const uint8_t* pcm_data, size_t bytes_len, const AudioFormatDesc& format)>;

class WasapiAudioCapture {
public:
    WasapiAudioCapture();
    ~WasapiAudioCapture();

    bool start(AudioDataCallback callback, const std::string& device_id = "");
    void stop();

    bool is_capturing() const { return is_capturing_; }
    AudioFormatDesc get_format() const { return format_; }

    std::vector<std::pair<std::string, std::string>> list_render_devices();

private:
    void capture_thread_func();

    std::atomic<bool> is_capturing_{false};
    std::thread capture_thread_;
    AudioDataCallback data_callback_;
    AudioFormatDesc format_;
    std::string device_id_;
};

} // namespace audiostream::audio

#endif // AUDIOSTREAM_WASAPI_CAPTURE_H
