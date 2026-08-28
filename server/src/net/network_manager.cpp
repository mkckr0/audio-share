#include "network_manager.h"
#include <iostream>
#include <thread>
#include <array>

namespace audiostream::net {

NetworkManager::NetworkManager(uint16_t port)
    : port_(port) {
    // Default 48kHz Stereo Opus 128kbps config
    active_config_.set_codec(com::audiostream::pb::CODEC_OPUS);
    active_config_.set_opus_bitrate(128000);
    active_config_.set_opus_frame_size_ms(20);
    active_config_.set_target_latency_ms(40);
    auto* fmt = active_config_.mutable_format();
    fmt->set_encoding(com::audiostream::pb::ENCODING_PCM_16BIT);
    fmt->set_channels(2);
    fmt->set_sample_rate(48000);
}

NetworkManager::~NetworkManager() {
    stop();
}

bool NetworkManager::start(PhoneMicAudioCallback mic_cb) {
    if (is_running_) stop();
    mic_callback_ = mic_cb;
    is_running_ = true;

    try {
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            io_context_,
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port_)
        );
        udp_socket_ = std::make_unique<asio::ip::udp::socket>(
            io_context_,
            asio::ip::udp::endpoint(asio::ip::udp::v4(), port_)
        );

        start_accept();
        start_udp_receive();

        io_thread_ = std::thread([this]() {
            asio::executor_work_guard<asio::io_context::executor_type> work(io_context_.get_executor());
            io_context_.run();
        });

        timeout_thread_ = std::thread([this]() {
            while (is_running_) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                check_peer_timeouts();
            }
        });

        return true;
    } catch (const std::exception& e) {
        std::cerr << "NetworkManager start failed: " << e.what() << std::endl;
        return false;
    }
}

void NetworkManager::stop() {
    is_running_ = false;
    io_context_.stop();
    if (io_thread_.joinable()) io_thread_.join();
    if (timeout_thread_.joinable()) timeout_thread_.join();

    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& [id, peer] : peers_) {
        if (peer.socket && peer.socket->is_open()) {
            asio::error_code ec;
            peer.socket->close(ec);
        }
    }
    peers_.clear();
}

void NetworkManager::start_accept() {
    if (!acceptor_ || !is_running_) return;
    auto sock = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_->async_accept(*sock, [this, sock](const asio::error_code& ec) {
        if (!ec && is_running_) {
            std::thread([this, sock]() { handle_tcp_session(sock); }).detach();
        }
        if (is_running_) start_accept();
    });
}

void NetworkManager::handle_tcp_session(std::shared_ptr<asio::ip::tcp::socket> sock) {
    uint32_t session_id = 0;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        session_id = next_session_id_++;
        ConnectedPeer peer;
        peer.session_id = session_id;
        peer.socket = sock;
        peer.ip_address = sock->remote_endpoint().address().to_string();
        peer.tcp_port = sock->remote_endpoint().port();
        peer.udp_port = port_;
        peer.udp_endpoint = asio::ip::udp::endpoint(sock->remote_endpoint().address(), port_);
        peer.last_heartbeat = std::chrono::steady_clock::now();
        peer.config = active_config_;
        peers_[session_id] = peer;
    }

    try {
        while (is_running_ && sock->is_open()) {
            uint32_t cmd = 0;
            asio::read(*sock, asio::buffer(&cmd, sizeof(cmd)));

            {
                std::lock_guard<std::mutex> lock(peers_mutex_);
                if (peers_.find(session_id) != peers_.end()) {
                    peers_[session_id].last_heartbeat = std::chrono::steady_clock::now();
                }
            }

            switch (cmd) {
                case 1: { // CMD_GET_FORMAT
                    com::audiostream::pb::AudioFormat fmt;
                    fmt.set_encoding(com::audiostream::pb::ENCODING_PCM_16BIT);
                    fmt.set_channels(2);
                    fmt.set_sample_rate(48000);
                    std::string payload;
                    fmt.SerializeToString(&payload);
                    uint32_t len = static_cast<uint32_t>(payload.size());
                    asio::write(*sock, asio::buffer(&len, sizeof(len)));
                    asio::write(*sock, asio::buffer(payload.data(), payload.size()));
                    break;
                }
                case 2: { // CMD_START_PLAY
                    int32_t resp_id = static_cast<int32_t>(session_id);
                    asio::write(*sock, asio::buffer(&resp_id, sizeof(resp_id)));
                    std::lock_guard<std::mutex> lock(peers_mutex_);
                    if (peers_.find(session_id) != peers_.end()) {
                        peers_[session_id].is_streaming = true;
                    }
                    break;
                }
                case 3: { // CMD_HEARTBEAT
                    uint32_t ack = 3;
                    asio::write(*sock, asio::buffer(&ack, sizeof(ack)));
                    break;
                }
                case 4: { // CMD_NEGOTIATE_CODEC
                    uint32_t len = 0;
                    asio::read(*sock, asio::buffer(&len, sizeof(len)));
                    std::vector<uint8_t> buf(len);
                    if (len > 0) {
                        asio::read(*sock, asio::buffer(buf.data(), len));
                    }
                    com::audiostream::pb::StreamConfig req;
                    if (req.ParseFromArray(buf.data(), static_cast<int>(len))) {
                        std::lock_guard<std::mutex> lock(peers_mutex_);
                        if (peers_.find(session_id) != peers_.end()) {
                            peers_[session_id].config = req;
                        }
                    }
                    // Ack with accepted config
                    std::string out_cfg;
                    active_config_.SerializeToString(&out_cfg);
                    uint32_t out_len = static_cast<uint32_t>(out_cfg.size());
                    asio::write(*sock, asio::buffer(&out_len, sizeof(out_len)));
                    asio::write(*sock, asio::buffer(out_cfg.data(), out_cfg.size()));
                    break;
                }
                default:
                    break;
            }
        }
    } catch (...) {
        // Socket disconnected
    }

    std::lock_guard<std::mutex> lock(peers_mutex_);
    peers_.erase(session_id);
}

void NetworkManager::start_udp_receive() {
    if (!udp_socket_ || !is_running_) return;

    auto buf = std::make_shared<std::array<uint8_t, 2048>>();
    auto sender_endpoint = std::make_shared<asio::ip::udp::endpoint>();

    udp_socket_->async_receive_from(
        asio::buffer(*buf),
        *sender_endpoint,
        [this, buf, sender_endpoint](const asio::error_code& ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd >= sizeof(UdpHeader) && is_running_) {
                const UdpHeader* hdr = reinterpret_cast<const UdpHeader*>(buf->data());
                if (hdr->packet_type == 0x02) { // Phone Mic Audio
                    const uint8_t* payload = buf->data() + sizeof(UdpHeader);
                    size_t payload_len = bytes_recvd - sizeof(UdpHeader);
                    
                    mic_jitter_buffer_.push_packet(hdr->packet_type, hdr->sequence, hdr->timestamp, payload, payload_len);

                    codec::AudioPacket out_pkt;
                    while (mic_jitter_buffer_.pop_frame(out_pkt)) {
                        int16_t pcm_out[960];
                        int decoded = mic_opus_decoder_.decode_int16(
                            out_pkt.is_plc ? nullptr : out_pkt.payload.data(),
                            out_pkt.is_plc ? 0 : static_cast<int>(out_pkt.payload.size()),
                            pcm_out,
                            960
                        );
                        if (decoded > 0 && mic_callback_) {
                            mic_callback_(reinterpret_cast<const uint8_t*>(pcm_out), decoded * sizeof(int16_t));
                        }
                    }
                }
            }
            if (is_running_) start_udp_receive();
        }
    );
}

void NetworkManager::broadcast_audio_frame(const uint8_t* data, size_t len, uint32_t timestamp) {
    if (!udp_socket_ || !is_running_ || !data || len == 0) return;

    std::vector<uint8_t> packet(sizeof(UdpHeader) + len);
    UdpHeader* hdr = reinterpret_cast<UdpHeader*>(packet.data());
    hdr->packet_type = 0x01; // Desktop PC Audio
    hdr->sequence = audio_sequence_++;
    hdr->timestamp = timestamp;
    std::memcpy(packet.data() + sizeof(UdpHeader), data, len);

    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& [id, peer] : peers_) {
        if (peer.is_streaming) {
            asio::error_code ec;
            udp_socket_->send_to(asio::buffer(packet.data(), packet.size()), peer.udp_endpoint, 0, ec);
            peer.packets_sent++;
            peer.bytes_sent += packet.size();
        }
    }
}

void NetworkManager::notify_start_mic(int32_t mic_source) {
    com::audiostream::pb::MicControl ctrl;
    ctrl.set_start(true);
    ctrl.set_mic_source(mic_source);
    *ctrl.mutable_config() = active_config_;

    std::string payload;
    ctrl.SerializeToString(&payload);
    uint32_t cmd = 5; // CMD_START_MIC
    uint32_t len = static_cast<uint32_t>(payload.size());

    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& [id, peer] : peers_) {
        if (peer.socket && peer.socket->is_open()) {
            asio::error_code ec;
            asio::write(*peer.socket, asio::buffer(&cmd, sizeof(cmd)), ec);
            asio::write(*peer.socket, asio::buffer(&len, sizeof(len)), ec);
            asio::write(*peer.socket, asio::buffer(payload.data(), payload.size()), ec);
            peer.is_mic_active = true;
        }
    }
}

void NetworkManager::notify_stop_mic() {
    uint32_t cmd = 6; // CMD_STOP_MIC
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& [id, peer] : peers_) {
        if (peer.socket && peer.socket->is_open()) {
            asio::error_code ec;
            asio::write(*peer.socket, asio::buffer(&cmd, sizeof(cmd)), ec);
            peer.is_mic_active = false;
        }
    }
}

void NetworkManager::check_peer_timeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint32_t> dead_peers;

    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (const auto& [id, peer] : peers_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - peer.last_heartbeat).count();
            if (elapsed > 6) { // 6.0s timeout
                dead_peers.push_back(id);
            }
        }
    }

    for (uint32_t id : dead_peers) {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto it = peers_.find(id);
        if (it != peers_.end()) {
            if (it->second.socket && it->second.socket->is_open()) {
                asio::error_code ec;
                it->second.socket->close(ec);
            }
            peers_.erase(it);
        }
    }
}

std::vector<ConnectedPeer> NetworkManager::get_connected_peers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<ConnectedPeer> list;
    for (const auto& [id, peer] : peers_) {
        list.push_back(peer);
    }
    return list;
}

size_t NetworkManager::get_peer_count() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peers_.size();
}

com::audiostream::pb::StreamConfig NetworkManager::get_active_config() const {
    return active_config_;
}

void NetworkManager::set_active_config(const com::audiostream::pb::StreamConfig& config) {
    active_config_ = config;
}

} // namespace audiostream::net
