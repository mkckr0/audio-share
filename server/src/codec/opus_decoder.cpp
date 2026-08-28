#include "opus_decoder.h"
#include <opus/opus.h>

namespace audiostream::codec {

AudioOpusDecoder::AudioOpusDecoder(int sample_rate, int channels)
    : sample_rate_(sample_rate), channels_(channels) {
    initialize(sample_rate, channels);
}

AudioOpusDecoder::~AudioOpusDecoder() {
    destroy();
}

bool AudioOpusDecoder::initialize(int sample_rate, int channels) {
    destroy();
    sample_rate_ = sample_rate;
    channels_ = channels;

    int error = 0;
    decoder_ = opus_decoder_create(sample_rate_, channels_, &error);
    if (error != OPUS_OK || !decoder_) {
        decoder_ = nullptr;
        return false;
    }
    return true;
}

void AudioOpusDecoder::destroy() {
    if (decoder_) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }
}

int AudioOpusDecoder::decode_float(const uint8_t* opus_data, int opus_len, float* out_pcm, int frame_size_samples_per_channel, bool fec) {
    if (!decoder_ || !out_pcm) return -1;
    if (!opus_data || opus_len <= 0) {
        // PLC Concealment
        return opus_decode_float(decoder_, nullptr, 0, out_pcm, frame_size_samples_per_channel, 0);
    }
    return opus_decode_float(decoder_, opus_data, opus_len, out_pcm, frame_size_samples_per_channel, fec ? 1 : 0);
}

int AudioOpusDecoder::decode_int16(const uint8_t* opus_data, int opus_len, int16_t* out_pcm, int frame_size_samples_per_channel, bool fec) {
    if (!decoder_ || !out_pcm) return -1;
    if (!opus_data || opus_len <= 0) {
        // PLC Concealment
        return opus_decode(decoder_, nullptr, 0, out_pcm, frame_size_samples_per_channel, 0);
    }
    return opus_decode(decoder_, opus_data, opus_len, out_pcm, frame_size_samples_per_channel, fec ? 1 : 0);
}

} // namespace audiostream::codec
