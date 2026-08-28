#include "jitter_buffer.h"
#include <cmath>
#include <algorithm>

namespace audiostream::codec {

AdaptiveJitterBuffer::AdaptiveJitterBuffer(JitterPreset preset, int frame_duration_ms)
    : preset_(preset), frame_duration_ms_(frame_duration_ms) {
    set_preset(preset);
}

void AdaptiveJitterBuffer::set_preset(JitterPreset preset) {
    std::lock_guard<std::mutex> lock(mutex_);
    preset_ = preset;
    switch (preset) {
        case JitterPreset::LOW_LATENCY:
            target_buffer_depth_ = 1; // ~20ms
            break;
        case JitterPreset::BALANCED:
            target_buffer_depth_ = 3; // ~60ms
            break;
        case JitterPreset::HIGH_RELIABILITY:
            target_buffer_depth_ = 5; // ~100ms
            break;
    }
}

void AdaptiveJitterBuffer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    initialized_ = false;
    next_expected_seq_ = 0;
    jitter_estimate_ = 0.0;
    stats_ = JitterStats();
}

bool AdaptiveJitterBuffer::push_packet(uint8_t packet_type, uint16_t sequence, uint32_t timestamp, const uint8_t* payload, size_t payload_len) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.packets_received++;

    if (!initialized_) {
        initialized_ = true;
        next_expected_seq_ = sequence;
    } else {
        int16_t diff = seq_diff(sequence, next_expected_seq_);
        if (diff < 0) {
            // Late packet older than playout point
            stats_.packets_dropped++;
            return false;
        }
        if (buffer_.find(sequence) != buffer_.end()) {
            // Duplicate packet
            stats_.duplicate_packets++;
            return false;
        }
    }

    update_jitter_rfc3550(timestamp);

    AudioPacket pkt;
    pkt.packet_type = packet_type;
    pkt.sequence = sequence;
    pkt.timestamp = timestamp;
    pkt.is_plc = false;
    if (payload && payload_len > 0) {
        pkt.payload.assign(payload, payload + payload_len);
    }

    buffer_[sequence] = std::move(pkt);

    // Limit maximum buffer capacity to prevent unbounded memory growth on stall
    while (buffer_.size() > 50) {
        buffer_.erase(buffer_.begin());
        stats_.packets_dropped++;
    }

    return true;
}

bool AdaptiveJitterBuffer::pop_frame(AudioPacket& out_frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) return false;

    auto it = buffer_.find(next_expected_seq_);
    if (it != buffer_.end()) {
        out_frame = std::move(it->second);
        buffer_.erase(it);
        next_expected_seq_++;
        return true;
    }

    // Sequence gap detected: if newer packets exist in buffer, synthesize PLC frame for missing packet
    if (!buffer_.empty()) {
        auto earliest_it = buffer_.begin();
        int16_t diff = seq_diff(earliest_it->first, next_expected_seq_);
        if (diff > 0) {
            // Missing packet in sequence
            out_frame.packet_type = earliest_it->second.packet_type;
            out_frame.sequence = next_expected_seq_++;
            out_frame.timestamp = 0;
            out_frame.payload.clear();
            out_frame.is_plc = true;
            stats_.packets_lost++;
            stats_.plc_frames_generated++;
            return true;
        }
    }

    return false;
}

void AdaptiveJitterBuffer::update_jitter_rfc3550(uint32_t timestamp) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint32_t arrival = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    if (last_transit_ != 0) {
        int32_t transit = static_cast<int32_t>(arrival - timestamp);
        int32_t d = transit - static_cast<int32_t>(last_transit_);
        if (d < 0) d = -d;
        jitter_estimate_ += (1.0 / 16.0) * (static_cast<double>(d) - jitter_estimate_);
        stats_.current_jitter_ms = jitter_estimate_;
    }
    last_transit_ = arrival - timestamp;
}

JitterStats AdaptiveJitterBuffer::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    JitterStats s = stats_;
    s.current_buffered_frames = buffer_.size();
    return s;
}

size_t AdaptiveJitterBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

} // namespace audiostream::codec
