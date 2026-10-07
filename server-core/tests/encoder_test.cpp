#define _USE_MATH_DEFINES
#include <opus.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <thread>

#include "opus_encoder.hpp"
#include "spsc_queue.hpp"

using Encoding = opus_stream_encoder::Encoding;
using PbFormat = io::github::mkckr0::audio_share_app::pb::AudioFormat;

static int g_failures = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            ++g_failures;                                 \
            std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                     \
            std::printf("\n");                            \
        }                                                 \
    } while (0)

// Power of everything that isn't the tone at f0 (least squares fit), relative to the tone, in dB.
static double residual_db(const std::vector<float>& x, size_t skip, double f0, double fs, int stride = 1, int channel = 0)
{
    double cc = 0, ss = 0, cs = 0, xc = 0, xs = 0, total = 0;
    for (size_t i = skip; i * stride + channel < x.size(); ++i) {
        const double v = x[i * stride + channel];
        const double ph = 2 * M_PI * f0 * (double)i / fs;
        const double c = std::cos(ph), s = std::sin(ph);
        cc += c * c; ss += s * s; cs += c * s;
        xc += v * c; xs += v * s;
        total += v * v;
    }
    const double det = cc * ss - cs * cs;
    const double a = (xc * ss - xs * cs) / det;
    const double b = (xs * cc - xc * cs) / det;
    const double tone_power = a * xc + b * xs;
    return 10.0 * std::log10(std::max(total - tone_power, 1e-30) / tone_power);
}

static std::vector<float> sine(double f0, double fs, size_t frames, int channels, float amp = 0.5f)
{
    std::vector<float> x(frames * channels);
    for (size_t i = 0; i < frames; ++i) {
        for (int c = 0; c < channels; ++c) {
            x[i * channels + c] = amp * (float)std::sin(2 * M_PI * f0 * (double)i / fs);
        }
    }
    return x;
}

static void test_resampler_quality()
{
    for (double f0 : { 1000.0, 10000.0, 15000.0, 19000.0 }) {
        sinc_resampler rs(44100, 48000, 1);
        auto in = sine(f0, 44100, 44100, 1);
        std::vector<float> out;
        rs.process(in.data(), in.size(), out);
        const double r = residual_db(out, 500, f0, 48000);
        CHECK(r < -80.0, "44.1k->48k %.0f Hz residual %.1f dB", f0, r);
    }
    for (double f0 : { 1000.0, 15000.0, 19000.0 }) {
        sinc_resampler rs(96000, 48000, 1);
        auto in = sine(f0, 96000, 96000, 1);
        std::vector<float> out;
        rs.process(in.data(), in.size(), out);
        const double r = residual_db(out, 500, f0, 48000);
        CHECK(r < -80.0, "96k->48k %.0f Hz residual %.1f dB", f0, r);
    }
    {
        sinc_resampler rs(96000, 48000, 1);
        auto in = sine(30000, 96000, 96000, 1);
        std::vector<float> out;
        rs.process(in.data(), in.size(), out);
        double peak = 0;
        for (size_t i = 500; i < out.size(); ++i) peak = std::max(peak, (double)std::abs(out[i]));
        CHECK(peak < 1e-3, "30 kHz leaked through 96k->48k, peak %.5f", peak);
    }
}

static void test_resampler_chunking()
{
    std::mt19937 rng(1);
    auto in = sine(5000, 44100, 20000, 2);
    sinc_resampler a(44100, 48000, 2), b(44100, 48000, 2);
    std::vector<float> whole, chunked;
    a.process(in.data(), in.size() / 2, whole);
    for (size_t pos = 0; pos < in.size() / 2;) {
        size_t n = std::min<size_t>(1 + rng() % 700, in.size() / 2 - pos);
        b.process(in.data() + pos * 2, n, chunked);
        pos += n;
    }
    CHECK(chunked.size() <= whole.size() && whole.size() - chunked.size() <= 4, "chunked produced %zu, whole %zu", chunked.size(), whole.size());
    double maxdiff = 0;
    for (size_t i = 0; i < chunked.size(); ++i) maxdiff = std::max(maxdiff, (double)std::abs(chunked[i] - whole[i]));
    CHECK(maxdiff < 1e-5, "chunk boundaries change the output, max diff %g", maxdiff);
}

static std::vector<float> roundtrip(Encoding enc, int channels, int rate, double f0, int bitrate, size_t& packets, int& pre_skip, int out_channels_expected)
{
    std::string err;
    auto e = opus_stream_encoder::create(enc, channels, rate, bitrate, err);
    CHECK(e != nullptr, "create failed: %s", err.c_str());
    if (!e) return {};
    CHECK(e->channels() == out_channels_expected, "output channels %d", e->channels());
    pre_skip = e->pre_skip();

    const size_t frames = (size_t)rate * 2;
    auto pcm = sine(f0, rate, frames, channels);
    std::vector<uint8_t> bytes;
    switch (enc) {
    case PbFormat::ENCODING_PCM_FLOAT:
        bytes.resize(pcm.size() * 4);
        std::memcpy(bytes.data(), pcm.data(), bytes.size());
        break;
    case PbFormat::ENCODING_PCM_16BIT:
        for (float v : pcm) {
            int16_t s = (int16_t)std::lrintf(v * 32767.0f);
            bytes.push_back(s & 0xff);
            bytes.push_back((s >> 8) & 0xff);
        }
        break;
    case PbFormat::ENCODING_PCM_24BIT:
        for (float v : pcm) {
            int32_t s = (int32_t)std::lrintf(v * 8388607.0f);
            bytes.push_back(s & 0xff);
            bytes.push_back((s >> 8) & 0xff);
            bytes.push_back((s >> 16) & 0xff);
        }
        break;
    default: break;
    }

    int derr = 0;
    OpusDecoder* dec = opus_decoder_create(48000, e->channels(), &derr);
    std::vector<float> decoded;
    std::vector<float> frame(960 * e->channels());
    std::mt19937 rng(7);
    packets = 0;
    const size_t frame_bytes = bytes.size() / frames;
    for (size_t pos = 0; pos < frames;) {
        size_t n = std::min<size_t>(1 + rng() % 2000, frames - pos);
        e->encode(bytes.data() + pos * frame_bytes, n * frame_bytes, [&](const uint8_t* p, size_t size) {
            ++packets;
            int got = opus_decode_float(dec, p, (opus_int32)size, frame.data(), 960, 0);
            CHECK(got == 960, "decode returned %d", got);
            if (got > 0) decoded.insert(decoded.end(), frame.begin(), frame.begin() + got * e->channels());
        });
        pos += n;
    }
    opus_decoder_destroy(dec);
    CHECK(e->errors() == 0, "encoder reported %zu errors", e->errors());
    return decoded;
}

static void test_roundtrip()
{
    struct Case { Encoding enc; int ch; int rate; const char* name; int out_ch; };
    const Case cases[] = {
        { PbFormat::ENCODING_PCM_FLOAT, 2, 48000, "f32 stereo 48k", 2 },
        { PbFormat::ENCODING_PCM_16BIT, 2, 44100, "s16 stereo 44.1k", 2 },
        { PbFormat::ENCODING_PCM_24BIT, 2, 48000, "s24 stereo 48k", 2 },
        { PbFormat::ENCODING_PCM_FLOAT, 1, 44100, "f32 mono 44.1k", 1 },
        { PbFormat::ENCODING_PCM_FLOAT, 6, 48000, "f32 5.1 48k", 2 },
    };
    for (auto& c : cases) {
        size_t packets = 0;
        int pre_skip = 0;
        auto out = roundtrip(c.enc, c.ch, c.rate, 1000.0, 128000, packets, pre_skip, c.out_ch);
        CHECK(packets >= 98 && packets <= 100, "%s: %zu packets for 2 s", c.name, packets);
        CHECK(pre_skip > 0 && pre_skip < 1000, "%s: pre_skip %d", c.name, pre_skip);
        if (out.empty()) continue;
        const double r = residual_db(out, 4800 + pre_skip, 1000.0, 48000, c.out_ch, 0);
        CHECK(r < -25.0, "%s: tone residual %.1f dB", c.name, r);
        std::printf("  %-18s packets=%zu pre_skip=%d residual=%.1f dB\n", c.name, packets, pre_skip, r);
    }
}

static void test_downmix_levels()
{
    std::string err;
    auto e = opus_stream_encoder::create(PbFormat::ENCODING_PCM_FLOAT, 6, 48000, 128000, err);
    CHECK(e != nullptr, "%s", err.c_str());
    for (int only : { 2, 3 }) {
        std::vector<float> in(48000 * 6, 0.0f);
        for (size_t i = 0; i < 48000; ++i) in[i * 6 + only] = 0.5f * (float)std::sin(2 * M_PI * 1000.0 * (double)i / 48000);
        auto enc = opus_stream_encoder::create(PbFormat::ENCODING_PCM_FLOAT, 6, 48000, 128000, err);
        OpusDecoder* dec = opus_decoder_create(48000, 2, nullptr);
        std::vector<float> out, frame(960 * 2);
        enc->encode((const uint8_t*)in.data(), in.size() * 4, [&](const uint8_t* p, size_t size) {
            int got = opus_decode_float(dec, p, (opus_int32)size, frame.data(), 960, 0);
            if (got > 0) out.insert(out.end(), frame.begin(), frame.begin() + got * 2);
        });
        opus_decoder_destroy(dec);
        double pl = 0, pr = 0;
        for (size_t i = 9600; i + 1 < out.size(); i += 2) { pl += out[i] * out[i]; pr += out[i + 1] * out[i + 1]; }
        if (only == 2) {
            CHECK(pl > 1.0 && std::abs(pl - pr) / pl < 0.05, "centre not mixed equally: %.2f vs %.2f", pl, pr);
        } else {
            CHECK(pl < 1e-3 && pr < 1e-3, "LFE leaked into the downmix: %g %g", pl, pr);
        }
    }
}

static void test_rejects()
{
    std::string err;
    CHECK(!opus_stream_encoder::create(PbFormat::ENCODING_INVALID, 2, 48000, 128000, err), "invalid encoding accepted");
    CHECK(!opus_stream_encoder::create(PbFormat::ENCODING_PCM_FLOAT, 9, 48000, 128000, err), "9 channels accepted");
    CHECK(!opus_stream_encoder::create(PbFormat::ENCODING_PCM_FLOAT, 2, 100, 128000, err), "100 Hz accepted");
}

static void test_spsc()
{
    spsc_byte_queue q(1 << 12);
    constexpr int count = 20000;
    std::atomic<bool> bad { false };
    std::thread consumer([&] {
        std::vector<uint8_t> rec;
        for (int i = 0; i < count;) {
            if (!q.pop(rec)) continue;
            const size_t want = 1 + (size_t)(i * 37) % 900;
            if (rec.size() != want) { bad = true; return; }
            for (uint8_t b : rec) if (b != (uint8_t)i) { bad = true; return; }
            ++i;
        }
    });
    for (int i = 0; i < count;) {
        std::vector<uint8_t> rec(1 + (size_t)(i * 37) % 900, (uint8_t)i);
        if (q.push(rec.data(), rec.size())) ++i;
    }
    consumer.join();
    CHECK(!bad, "spsc queue corrupted or reordered a record");

    spsc_byte_queue small(64);
    std::vector<uint8_t> big(100, 1);
    CHECK(!small.push(big.data(), big.size()), "oversized push accepted");
}

int main()
{
    test_resampler_quality();
    test_resampler_chunking();
    test_roundtrip();
    test_downmix_levels();
    test_rejects();
    test_spsc();
    std::printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
