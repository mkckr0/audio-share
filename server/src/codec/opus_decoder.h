#ifndef AUDIOSTREAM_OPUS_DECODER_H
#define AUDIOSTREAM_OPUS_DECODER_H

#include <vector>
#include <cstdint>
#include <memory>

struct OpusDecoder;

namespace audiostream::codec {

class AudioOpusDecoder {
public:
    AudioOpusDecoder(int sample_rate = 48000, int channels = 1);
    ~AudioOpusDecoder();

    bool initialize(int sample_rate = 48000, int channels = 1);
    void destroy();

    // Decode Opus packet to float PCM. If opus_data is null or len == 0, PLC (packet loss concealment) is executed.
    int decode_float(const uint8_t* opus_data, int opus_len, float* out_pcm, int frame_size_samples_per_channel, bool fec = false);

    // Decode Opus packet to 16-bit PCM. If opus_data is null or len == 0, PLC is executed.
    int decode_int16(const uint8_t* opus_data, int opus_len, int16_t* out_pcm, int frame_size_samples_per_channel, bool fec = false);

    int get_sample_rate() const { return sample_rate_; }
    int get_channels() const { return channels_; }
    bool is_initialized() const { return decoder_ != nullptr; }

private:
    OpusDecoder* decoder_ = nullptr;
    int sample_rate_ = 48000;
    int channels_ = 1;
};

} // namespace audiostream::codec

#endif // AUDIOSTREAM_OPUS_DECODER_H
