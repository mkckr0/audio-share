#include "opus_encoder.h"
#include <opus/opus.h>
#include <iostream>

namespace audiostream::codec {

AudioOpusEncoder::AudioOpusEncoder(int sample_rate, int channels, int bitrate_bps)
    : sample_rate_(sample_rate), channels_(channels), bitrate_bps_(bitrate_bps) {
    initialize(sample_rate, channels, bitrate_bps);
}

AudioOpusEncoder::~AudioOpusEncoder() {
    destroy();
}

bool AudioOpusEncoder::initialize(int sample_rate, int channels, int bitrate_bps) {
    destroy();
    sample_rate_ = sample_rate;
    channels_ = channels;
    bitrate_bps_ = bitrate_bps;

    int error = 0;
    encoder_ = opus_encoder_create(sample_rate_, channels_, OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK || !encoder_) {
        encoder_ = nullptr;
        return false;
    }

    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitrate_bps_));
    opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(encoder_, OPUS_SET_PACKET_LOSS_PERC(5));
    opus_encoder_ctl(encoder_, OPUS_SET_VBR(1));
    opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(8));
    opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

    return true;
}

void AudioOpusEncoder::destroy() {
    if (encoder_) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }
}

bool AudioOpusEncoder::set_bitrate(int bitrate_bps) {
    if (!encoder_) return false;
    bitrate_bps_ = bitrate_bps;
    return opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitrate_bps)) == OPUS_OK;
}

bool AudioOpusEncoder::set_fec(bool enable) {
    if (!encoder_) return false;
    return opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(enable ? 1 : 0)) == OPUS_OK;
}

bool AudioOpusEncoder::set_vbr(bool enable) {
    if (!encoder_) return false;
    return opus_encoder_ctl(encoder_, OPUS_SET_VBR(enable ? 1 : 0)) == OPUS_OK;
}

int AudioOpusEncoder::encode_float(const float* pcm, int frame_size_samples_per_channel, uint8_t* out_opus, int max_out_bytes) {
    if (!encoder_ || !pcm || !out_opus) return -1;
    return opus_encode_float(encoder_, pcm, frame_size_samples_per_channel, out_opus, max_out_bytes);
}

int AudioOpusEncoder::encode_int16(const int16_t* pcm, int frame_size_samples_per_channel, uint8_t* out_opus, int max_out_bytes) {
    if (!encoder_ || !pcm || !out_opus) return -1;
    return opus_encode(encoder_, pcm, frame_size_samples_per_channel, out_opus, max_out_bytes);
}

} // namespace audiostream::codec
