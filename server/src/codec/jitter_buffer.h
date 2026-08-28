#ifndef AUDIOSTREAM_JITTER_BUFFER_H
#define AUDIOSTREAM_JITTER_BUFFER_H

#include <cstdint>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>

namespace audiostream::codec {

inline int16_t seq_diff(uint16_t a, uint16_t b) {
    return static_cast<int16_t>(a - b);
}

enum class JitterPreset {
    LOW_LATENCY,    // ~20ms target buffer
    BALANCED,       // ~50ms target buffer
    HIGH_RELIABILITY // ~100ms target buffer
};

struct AudioPacket {
    uint8_t packet_type = 0;
    uint16_t sequence = 0;
    uint32_t timestamp = 0;
    std::vector<uint8_t> payload;
    bool is_plc = false;
};

struct JitterStats {
    uint64_t packets_received = 0;
    uint64_t packets_lost = 0;
    uint64_t packets_dropped = 0;
    uint64_t duplicate_packets = 0;
    uint64_t plc_frames_generated = 0;
    double current_jitter_ms = 0.0;
    double smoothed_rtt_ms = 0.0;
    size_t current_buffered_frames = 0;
};

class AdaptiveJitterBuffer {
public:
    AdaptiveJitterBuffer(JitterPreset preset = JitterPreset::BALANCED, int frame_duration_ms = 20);
    ~AdaptiveJitterBuffer() = default;

    void set_preset(JitterPreset preset);
    void reset();

    // Push an incoming network UDP packet into the jitter buffer
    bool push_packet(uint8_t packet_type, uint16_t sequence, uint32_t timestamp, const uint8_t* payload, size_t payload_len);

    // Pop the next sequential frame ready for decoding/rendering
    bool pop_frame(AudioPacket& out_frame);

    JitterStats get_stats() const;
    size_t size() const;

private:
    void update_jitter_rfc3550(uint32_t timestamp);

    mutable std::mutex mutex_;
    JitterPreset preset_ = JitterPreset::BALANCED;
    int frame_duration_ms_ = 20;
    size_t target_buffer_depth_ = 3; // Number of frames

    bool initialized_ = false;
    uint16_t next_expected_seq_ = 0;
    uint32_t last_transit_ = 0;
    double jitter_estimate_ = 0.0;

    std::map<uint16_t, AudioPacket> buffer_;
    JitterStats stats_;
};

} // namespace audiostream::codec

#endif // AUDIOSTREAM_JITTER_BUFFER_H
