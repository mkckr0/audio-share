#ifndef AUDIOSTREAM_OPUS_ENCODER_H
#define AUDIOSTREAM_OPUS_ENCODER_H

#include <vector>
#include <cstdint>
#include <memory>
#include <string>

struct OpusEncoder;

namespace audiostream::codec {

class AudioOpusEncoder {
public:
    AudioOpusEncoder(int sample_rate = 48000, int channels = 2, int bitrate_bps = 128000);
    ~AudioOpusEncoder();

    bool initialize(int sample_rate = 48000, int channels = 2, int bitrate_bps = 128000);
    void destroy();

    bool set_bitrate(int bitrate_bps);
    bool set_fec(bool enable);
    bool set_vbr(bool enable);

    // Encode float PCM samples [-1.0f, 1.0f] into Opus packet
    int encode_float(const float* pcm, int frame_size_samples_per_channel, uint8_t* out_opus, int max_out_bytes);

    // Encode 16-bit signed integer PCM samples into Opus packet
    int encode_int16(const int16_t* pcm, int frame_size_samples_per_channel, uint8_t* out_opus, int max_out_bytes);

    int get_sample_rate() const { return sample_rate_; }
    int get_channels() const { return channels_; }
    int get_bitrate() const { return bitrate_bps_; }
    bool is_initialized() const { return encoder_ != nullptr; }

private:
    OpusEncoder* encoder_ = nullptr;
    int sample_rate_ = 48000;
    int channels_ = 2;
    int bitrate_bps_ = 128000;
};

} // namespace audiostream::codec

#endif // AUDIOSTREAM_OPUS_ENCODER_H
