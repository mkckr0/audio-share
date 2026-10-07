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

#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <mutex>
#include <atomic>
#include <random>

#include "pre_asio.hpp"
#include <asio.hpp>
#include <asio/use_awaitable.hpp>

#include "audio_manager.hpp"
#include "opus_encoder.hpp"
#include "spsc_queue.hpp"
#include "silence_detector.hpp"

class network_manager : public std::enable_shared_from_this<network_manager>
{
    using default_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using tcp_acceptor = default_token::as_default_on_t<asio::ip::tcp::acceptor>;
    using tcp_socket = default_token::as_default_on_t<asio::ip::tcp::socket>;
    using udp_socket = default_token::as_default_on_t<asio::ip::udp::socket>;
    using steady_timer = default_token::as_default_on_t<asio::steady_timer>;

    struct peer_info_t {
        int id = 0;
        bool opus = false;
        asio::ip::address tcp_address;
        asio::ip::udp::endpoint udp_peer; // port 0 until the client registers
        std::chrono::steady_clock::time_point last_tick;
    };

    using playing_peer_list_t = std::map<std::shared_ptr<tcp_socket>, std::shared_ptr<peer_info_t>>;

    enum class cmd_t : uint32_t {
        cmd_none = 0,
        cmd_get_format = 1,
        cmd_start_play = 2,
        cmd_heartbeat = 3,
        cmd_get_format_v2 = 4, // same as cmd_get_format, preceded by the client's capability bits
    };

public:
    static constexpr uint32_t cap_opus = 1u << 0;

private:

public:

    explicit network_manager(std::shared_ptr<audio_manager>& audio_manager);

    static std::vector<std::string> get_address_list();
    static std::string get_default_address();
private:
    static std::string select_default_address(const std::vector<std::string>& address_list);

public:
    void start_server(const std::string& host, uint16_t port, const audio_manager::capture_config& capture_config);
    void stop_server();
    void wait_server();
    bool is_running() const;

private:
    asio::awaitable<void> accept_tcp_loop(tcp_acceptor acceptor);
    asio::awaitable<void> read_loop(std::shared_ptr<tcp_socket> peer);
    asio::awaitable<void> heartbeat_loop(std::shared_ptr<tcp_socket> peer);
    asio::awaitable<void> accept_udp_loop();
    
    playing_peer_list_t::iterator close_session(std::shared_ptr<tcp_socket>& peer);
    int add_playing_peer(std::shared_ptr<tcp_socket>& peer, bool opus);
    playing_peer_list_t::iterator remove_playing_peer(std::shared_ptr<tcp_socket>& peer);
    void fill_udp_peer(int id, asio::ip::udp::endpoint udp_peer);
    void update_peer_counts();

    using segment_list_t = std::list<std::shared_ptr<std::vector<uint8_t>>>;
    bool ensure_codec();
    std::string get_format_binary_for(uint32_t client_caps, bool& opus);
    void send_segments(segment_list_t pcm_segments, segment_list_t opus_segments);
    void encode_loop();
    void process_audio(const uint8_t* data, size_t count, int block_align);
    bool should_transmit(const char* data, size_t count);

public:
    void broadcast_audio_data(const char* data, size_t count, int block_align);
    
    std::shared_ptr<asio::io_context> _ioc;

private:
    std::shared_ptr<audio_manager> _audio_manager;
    std::thread _net_thread;
    std::unique_ptr<udp_socket> _udp_server;
    playing_peer_list_t _playing_peer_list;
    constexpr static auto _heartbeat_timeout = std::chrono::seconds(5);

    // Compression state. Requested in start_server(), resolved lazily once the
    // capture format is known (see ensure_codec()).
    audio_manager::compression_t _requested_compression = audio_manager::compression_t::compression_none;
    int _requested_bitrate = 0;
    std::mutex _codec_mutex;
    bool _codec_resolved = false;
    audio_manager::AudioFormat _raw_format;
    audio_manager::AudioFormat _opus_format;

    // Silence gating (only touched from the encode thread). While the captured audio
    // stays silent for `_silence_timeout`, nothing is sent so the client can idle.
    std::chrono::milliseconds _silence_timeout { 0 };
    audio_manager::AudioFormat::Encoding _capture_encoding = audio_manager::AudioFormat::ENCODING_INVALID;
    bool _silent_tracking = false;
    bool _idle = false;
    std::chrono::steady_clock::time_point _silent_since;
#ifdef AUDIO_SHARE_WITH_OPUS
    std::unique_ptr<opus_stream_encoder> _opus_encoder;
#endif
    uint16_t _opus_seq = 0; // encode thread only

    // written on the net thread, read by the capture and encode threads to skip unneeded work
    std::atomic<int> _opus_peer_count { 0 };
    std::atomic<int> _pcm_peer_count { 0 };

    std::mt19937 _id_rng { std::random_device {}() };

    // the capture callback only copies into the queue; encoding and sending run on _encode_thread
    spsc_byte_queue _capture_queue { 1u << 21 };
    std::thread _encode_thread;
    std::atomic<uint32_t> _capture_signal { 0 };
    std::atomic<bool> _encode_stop { false };
    std::atomic<int> _block_align { 1 };
    std::atomic<size_t> _dropped_chunks { 0 };
};

#endif // !NETWORK_MANAGER_HPP