/*
   Copyright 2022-2024 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef OPUS_ENCODER_HPP
#define OPUS_ENCODER_HPP

#ifdef AUDIO_SHARE_WITH_OPUS

#include <opus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "client.pb.h"

class sinc_resampler {
public:
    static constexpr int zero_crossings = 16;
    static constexpr int phases = 256;
    static constexpr double kaiser_beta = 9.0;
    static constexpr double pi = 3.14159265358979323846; // MSVC has no M_PI without a define

    sinc_resampler(int in_rate, int out_rate, int channels)
        : _channels(channels)
        , _step((double)in_rate / out_rate)
    {
        // cutoff sits just below the lower Nyquist so downsampling doesn't alias
        const double cutoff = 0.5 * std::min(1.0, (double)out_rate / in_rate) * 0.96;
        const double half_width = zero_crossings / (2 * cutoff);
        _taps_per_side = (int)std::ceil(half_width);

        _table.resize((size_t)phases * _taps_per_side + 2, 0.0f);
        const double i0_beta = bessel_i0(kaiser_beta);
        for (size_t k = 0; k < _table.size(); ++k) {
            const double t = (double)k / phases;
            const double r = std::min(t / half_width, 1.0);
            const double window = bessel_i0(kaiser_beta * std::sqrt(1.0 - r * r)) / i0_beta;
            const double x = 2 * cutoff * t;
            const double sinc = x == 0.0 ? 1.0 : std::sin(pi * x) / (pi * x);
            _table[k] = (float)(2 * cutoff * sinc * window);
        }

        _in.assign((size_t)_taps_per_side * channels, 0.0f);
        _pos = _taps_per_side;
        _coef.resize((size_t)_taps_per_side * 2);
    }

    void process(const float* in, size_t frames, std::vector<float>& out)
    {
        _in.insert(_in.end(), in, in + frames * _channels);
        const size_t have = _in.size() / _channels;
        const size_t n = _taps_per_side;

        while ((size_t)_pos + n < have) {
            const size_t i = (size_t)_pos;
            const size_t first = i - (n - 1);

            for (size_t j = 0; j < 2 * n; ++j) {
                const double d = std::abs(_pos - (double)(first + j)) * phases;
                const size_t d0 = (size_t)d;
                const float frac = (float)(d - (double)d0);
                _coef[j] = _table[d0] + (_table[d0 + 1] - _table[d0]) * frac;
            }
            for (int c = 0; c < _channels; ++c) {
                float acc = 0.0f;
                for (size_t j = 0; j < 2 * n; ++j) {
                    acc += _in[(first + j) * _channels + c] * _coef[j];
                }
                out.push_back(acc);
            }
            _pos += _step;
        }

        const size_t drop = (size_t)_pos - (n - 1);
        _in.erase(_in.begin(), _in.begin() + drop * _channels);
        _pos -= (double)drop;
    }

private:
    static double bessel_i0(double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k) {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    int _channels;
    double _step;
    int _taps_per_side = 0;
    double _pos = 0.0;
    std::vector<float> _table;
    std::vector<float> _coef;
    std::vector<float> _in;
};

// Output is always 48 kHz because Android's decoder only does 48 kHz, so other rates get resampled. One thread only.
class opus_stream_encoder {
public:
    using Encoding = io::github::mkckr0::audio_share_app::pb::AudioFormat::Encoding;

    static constexpr int frame_ms = 20;
    static constexpr int max_packet_size = 1275; // maximum size of one Opus packet

    static constexpr int output_rate = 48000;

    /** Returns nullptr (and sets `error`) if the format can't be compressed. */
    static std::unique_ptr<opus_stream_encoder> create(Encoding encoding, int channels, int sample_rate, int bitrate, std::string& error)
    {
        if (channels < 1 || channels > 8) {
            error = "opus compression supports 1 to 8 channels";
            return nullptr;
        }
        if (bytes_per_sample(encoding) == 0) {
            error = "unsupported PCM encoding";
            return nullptr;
        }
        if (sample_rate < 8000 || sample_rate > 384000) {
            error = "unsupported sample rate";
            return nullptr;
        }

        auto e = std::unique_ptr<opus_stream_encoder>(new opus_stream_encoder());
        e->_encoding = encoding;
        e->_in_channels = channels;
        e->_channels = std::min(channels, 2);
        e->_in_rate = sample_rate;
        e->_frame_samples = output_rate * frame_ms / 1000;
        if (channels > 2) {
            e->_downmix = make_downmix(channels);
        }
        if (sample_rate != output_rate) {
            e->_resampler = std::make_unique<sinc_resampler>(sample_rate, output_rate, e->_channels);
        }

        int err = OPUS_OK;
        e->_enc = opus_encoder_create(output_rate, e->_channels, OPUS_APPLICATION_AUDIO, &err);
        if (err != OPUS_OK || !e->_enc) {
            error = opus_strerror(err);
            return nullptr;
        }
        opus_encoder_ctl(e->_enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
        opus_encoder_ctl(e->_enc, OPUS_SET_COMPLEXITY(10));
        opus_encoder_ctl(e->_enc, OPUS_SET_VBR(1));
        opus_encoder_ctl(e->_enc, OPUS_SET_VBR_CONSTRAINT(1)); // keeps packet sizes predictable on Wi-Fi
        if (bitrate > 0) {
            opus_encoder_ctl(e->_enc, OPUS_SET_BITRATE(bitrate));
        }
        opus_encoder_ctl(e->_enc, OPUS_GET_LOOKAHEAD(&e->_lookahead));
        return e;
    }

    ~opus_stream_encoder()
    {
        if (_enc) {
            opus_encoder_destroy(_enc);
        }
    }

    opus_stream_encoder(const opus_stream_encoder&) = delete;
    opus_stream_encoder& operator=(const opus_stream_encoder&) = delete;

    int output_sample_rate() const { return output_rate; }
    int channels() const { return _channels; }
    int pre_skip() const { return _lookahead; }
    size_t errors() const { return _errors; }

    /**
     * Feed raw PCM. `emit(const uint8_t* packet, size_t size)` is invoked once per
     * completed 20 ms Opus packet. Leftover audio (<20 ms) is kept for the next call.
     */
    template <typename Emit>
    void encode(const uint8_t* data, size_t count, Emit&& emit)
    {
        const int bps = bytes_per_sample(_encoding);
        const size_t samples = count / bps / _in_channels * _in_channels;
        convert_to_float(data, samples);

        const float* pcm = _conv.data();
        size_t frames = samples / _in_channels;
        if (_in_channels > 2) {
            downmix(frames);
            pcm = _stereo.data();
        }

        if (_resampler) {
            _resampler->process(pcm, frames, _pcm);
        } else {
            _pcm.insert(_pcm.end(), pcm, pcm + frames * _channels);
        }

        const size_t frame_len = (size_t)_frame_samples * _channels;
        size_t offset = 0;
        uint8_t packet[max_packet_size];
        while (_pcm.size() - offset >= frame_len) {
            int n = opus_encode_float(_enc, _pcm.data() + offset, _frame_samples, packet, (opus_int32)sizeof(packet));
            offset += frame_len;
            if (n > 1) { // n == 1 means DTX, nothing worth sending
                emit((const uint8_t*)packet, (size_t)n);
            } else if (n < 0) {
                ++_errors;
            }
        }
        _pcm.erase(_pcm.begin(), _pcm.begin() + offset);
    }

private:
    opus_stream_encoder() = default;

    static int bytes_per_sample(Encoding e)
    {
        using E = io::github::mkckr0::audio_share_app::pb::AudioFormat;
        switch (e) {
        case E::ENCODING_PCM_8BIT: return 1;
        case E::ENCODING_PCM_16BIT: return 2;
        case E::ENCODING_PCM_24BIT: return 3;
        case E::ENCODING_PCM_32BIT:
        case E::ENCODING_PCM_FLOAT: return 4;
        default: return 0;
        }
    }

    // rows are L and R, columns follow WAVEFORMATEX order; each row sums to 1 so full scale can't clip
    static std::vector<float> make_downmix(int channels)
    {
        constexpr float k = 0.7071f;
        std::vector<float> l(channels, 0.0f), r(channels, 0.0f);
        l[0] = 1.0f;
        r[1] = 1.0f;
        auto center = [&](int c) { l[c] = r[c] = k; };
        auto left = [&](int c) { l[c] = k; };
        auto right = [&](int c) { r[c] = k; };
        switch (channels) {
        case 3: center(2); break;
        case 4: left(2); right(3); break;                                  // quad: FL FR BL BR
        case 5: center(2); left(3); right(4); break;                       // FL FR FC BL BR
        case 6: center(2); left(4); right(5); break;                       // 5.1: FL FR FC LFE BL BR
        case 7: center(2); left(4); right(5); l[6] = r[6] = k * k; break;  // 6.1: ... BC
        case 8: center(2); left(4); right(5); left(6); right(7); break;    // 7.1: ... SL SR
        default: break;
        }
        std::vector<float> m;
        for (auto* row : { &l, &r }) {
            float sum = 0.0f;
            for (float v : *row) sum += v;
            for (float v : *row) m.push_back(v / sum);
        }
        return m;
    }

    void downmix(size_t frames)
    {
        _stereo.resize(frames * 2);
        const float* l = _downmix.data();
        const float* r = l + _in_channels;
        for (size_t f = 0; f < frames; ++f) {
            const float* in = &_conv[f * _in_channels];
            float sl = 0.0f, sr = 0.0f;
            for (int c = 0; c < _in_channels; ++c) {
                sl += in[c] * l[c];
                sr += in[c] * r[c];
            }
            _stereo[2 * f] = sl;
            _stereo[2 * f + 1] = sr;
        }
    }

    // All PCM encodings are little endian (see client.proto); 8 bit is unsigned.
    void convert_to_float(const uint8_t* p, size_t samples)
    {
        using E = io::github::mkckr0::audio_share_app::pb::AudioFormat;
        _conv.resize(samples);
        switch (_encoding) {
        case E::ENCODING_PCM_8BIT:
            for (size_t i = 0; i < samples; ++i) {
                _conv[i] = ((int)p[i] - 128) / 128.0f;
            }
            break;
        case E::ENCODING_PCM_16BIT:
            for (size_t i = 0; i < samples; ++i) {
                int16_t v = (int16_t)(p[2 * i] | (p[2 * i + 1] << 8));
                _conv[i] = v / 32768.0f;
            }
            break;
        case E::ENCODING_PCM_24BIT:
            for (size_t i = 0; i < samples; ++i) {
                int32_t v = (int32_t)((uint32_t)p[3 * i] << 8 | (uint32_t)p[3 * i + 1] << 16 | (uint32_t)p[3 * i + 2] << 24) >> 8;
                _conv[i] = v / 8388608.0f;
            }
            break;
        case E::ENCODING_PCM_32BIT:
            for (size_t i = 0; i < samples; ++i) {
                uint32_t u = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
                _conv[i] = (int32_t)u / 2147483648.0f;
            }
            break;
        case E::ENCODING_PCM_FLOAT:
            std::memcpy(_conv.data(), p, samples * sizeof(float));
            break;
        default:
            std::fill(_conv.begin(), _conv.end(), 0.0f);
            break;
        }
    }

    OpusEncoder* _enc = nullptr;
    Encoding _encoding {};
    int _in_channels = 0;
    int _channels = 0;
    int _in_rate = 0;
    int _frame_samples = 0;
    int _lookahead = 0;
    size_t _errors = 0;
    std::vector<float> _downmix;
    std::unique_ptr<sinc_resampler> _resampler;
    std::vector<float> _conv;
    std::vector<float> _stereo;
    std::vector<float> _pcm;
};

#endif // AUDIO_SHARE_WITH_OPUS

#endif // !OPUS_ENCODER_HPP
