#ifndef AUDIOSTREAM_NETWORK_MANAGER_H
#define AUDIOSTREAM_NETWORK_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <asio.hpp>
#include "audiostream.pb.h"
#include "../codec/jitter_buffer.h"
#include "../codec/opus_encoder.h"
#include "../codec/opus_decoder.h"

namespace audiostream::net {

#pragma pack(push, 1)
struct UdpHeader {
    uint8_t packet_type;
    uint16_t sequence;
    uint32_t timestamp;
};
#pragma pack(pop)

static_assert(sizeof(UdpHeader) == 7, "UdpHeader must be exactly 7 bytes packed");

struct ConnectedPeer {
    uint32_t session_id = 0;
    std::string ip_address;
    uint16_t tcp_port = 0;
    uint16_t udp_port = 0;
    asio::ip::udp::endpoint udp_endpoint;
    std::shared_ptr<asio::ip::tcp::socket> socket;
    std::chrono::steady_clock::time_point last_heartbeat;
    bool is_streaming = false;
    bool is_mic_active = false;
    com::audiostream::pb::StreamConfig config;
    uint64_t packets_sent = 0;
    uint64_t bytes_sent = 0;
    uint64_t packets_recv = 0;
    uint64_t bytes_recv = 0;
};

using PhoneMicAudioCallback = std::function<void(const uint8_t* pcm_data, size_t bytes_len)>;

class NetworkManager {
public:
    NetworkManager(uint16_t port = 65530);
    ~NetworkManager();

    bool start(PhoneMicAudioCallback mic_cb = nullptr);
    void stop();

    // Broadcast PC audio packet to all streaming clients over UDP
    void broadcast_audio_frame(const uint8_t* data, size_t len, uint32_t timestamp);

    // Send control commands to all clients or specific client
    void notify_start_mic(int32_t mic_source = 7);
    void notify_stop_mic();

    std::vector<ConnectedPeer> get_connected_peers() const;
    size_t get_peer_count() const;
    bool is_running() const { return is_running_; }

    com::audiostream::pb::StreamConfig get_active_config() const;
    void set_active_config(const com::audiostream::pb::StreamConfig& config);

private:
    void start_accept();
    void start_udp_receive();
    void handle_tcp_session(std::shared_ptr<asio::ip::tcp::socket> sock);
    void check_peer_timeouts();

    uint16_t port_ = 65530;
    std::atomic<bool> is_running_{false};
    PhoneMicAudioCallback mic_callback_;

    asio::io_context io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<asio::ip::udp::socket> udp_socket_;
    std::thread io_thread_;
    std::thread timeout_thread_;

    mutable std::mutex peers_mutex_;
    std::map<uint32_t, ConnectedPeer> peers_;
    uint32_t next_session_id_ = 1000;

    uint16_t audio_sequence_ = 0;
    com::audiostream::pb::StreamConfig active_config_;

    // Server-side Mic Jitter Buffer & Opus Decoder
    codec::AdaptiveJitterBuffer mic_jitter_buffer_;
    codec::AudioOpusDecoder mic_opus_decoder_{48000, 1};
};

} // namespace audiostream::net

#endif // AUDIOSTREAM_NETWORK_MANAGER_H
