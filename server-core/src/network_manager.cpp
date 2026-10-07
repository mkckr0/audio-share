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

#include "network_manager.hpp"
#include "formatter.hpp"
#include "audio_manager.hpp"

#include <algorithm>
#include <list>
#include <ranges>
#include <coroutine>

#ifdef _WINDOWS
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif // _WINDOWS

#ifdef linux
#include <sys/types.h>
#include <ifaddrs.h>
#endif

#include <spdlog/spdlog.h>
#include <fmt/ranges.h>

namespace ip = asio::ip;
using namespace std::chrono_literals;

network_manager::network_manager(std::shared_ptr<audio_manager>& audio_manager)
    : _audio_manager(audio_manager)
{
}

std::vector<std::string> network_manager::get_address_list()
{
    std::vector<std::string> address_list;

#ifdef _WINDOWS
    ULONG family = AF_INET;
    ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;

    ULONG size = 0;
    GetAdaptersAddresses(family, flags, nullptr, nullptr, &size);
    auto pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(size);

    auto ret = GetAdaptersAddresses(family, flags, nullptr, pAddresses, &size);
    if (ret == ERROR_SUCCESS) {
        for (auto pCurrentAddress = pAddresses; pCurrentAddress; pCurrentAddress = pCurrentAddress->Next) {
            if (pCurrentAddress->OperStatus != IfOperStatusUp || pCurrentAddress->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                continue;
            }

            for (auto pUnicast = pCurrentAddress->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
                auto sockaddr = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                char buf[50];
                if (inet_ntop(AF_INET, &sockaddr->sin_addr, buf, sizeof(buf))) {
                    address_list.emplace_back(buf);
                }
            }
        }
    }

    free(pAddresses);
#endif

#ifdef linux
    struct ifaddrs* ifaddrs;
    if (getifaddrs(&ifaddrs) == -1) {
        return address_list;
    }

    for (auto ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (ifa->ifa_flags & IFF_LOOPBACK) {
            continue;
        }
        auto sockaddr = (sockaddr_in*)ifa->ifa_addr;
        char buf[50];
        if (inet_ntop(AF_INET, &sockaddr->sin_addr, buf, sizeof(buf))) {
            address_list.emplace_back(buf);
        }
    }

    freeifaddrs(ifaddrs);
#endif

    return address_list;
}

std::string network_manager::get_default_address()
{
    return select_default_address(get_address_list());
}

std::string network_manager::select_default_address(const std::vector<std::string>& address_list)
{
    if (address_list.empty()) {
        return {};
    }

    auto is_private_address = [](const std::string& address) {
        constexpr uint32_t private_addr_list[] = {
            0x0a000000,
            0xac100000,
            0xc0a80000,
        };

        uint32_t addr;
        inet_pton(AF_INET, address.c_str(), &addr);
        addr = ntohl(addr);
        for (auto&& private_addr : private_addr_list) {
            if ((addr & private_addr) == private_addr) {
                return true;
            }
        }

        return false;
    };

    for (auto&& address : address_list) {
        if (is_private_address(address)) {
            return address;
        }
    }
    return address_list.front();
}

void network_manager::start_server(const std::string& host, uint16_t port, const audio_manager::capture_config& capture_config)
{
    {
        std::lock_guard lock(_codec_mutex);
        _requested_compression = capture_config.compression;
        _requested_bitrate = capture_config.bitrate;
        _codec_resolved = false;
        _silence_timeout = std::chrono::milliseconds(std::max(0, capture_config.silence_timeout_ms));
        _silent_tracking = false;
        _idle = false;
#ifdef AUDIO_SHARE_WITH_OPUS
        _opus_encoder.reset();
#endif
        _opus_seq = 0;
    }
    _opus_peer_count = 0;
    _pcm_peer_count = 0;
#ifndef AUDIO_SHARE_WITH_OPUS
    if (capture_config.compression == audio_manager::compression_t::compression_opus) {
        spdlog::warn("this build has no Opus support, audio will be sent uncompressed");
    }
#endif

    _ioc = std::make_shared<asio::io_context>();
    {
        ip::tcp::endpoint endpoint { ip::make_address(host), port };

        ip::tcp::acceptor acceptor(*_ioc, endpoint.protocol());
        acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();

        _audio_manager->start_loopback_recording(shared_from_this(), capture_config);
        asio::co_spawn(*_ioc, accept_tcp_loop(std::move(acceptor)), asio::detached);

        // start tcp success
        spdlog::info("tcp listen success on {}", endpoint);
    }

    {
        ip::udp::endpoint endpoint { ip::make_address(host), port };
        _udp_server = std::make_unique<udp_socket>(*_ioc, endpoint.protocol());
        _udp_server->bind(endpoint);

        // DSCP EF lands in the WMM voice queue on Wi-Fi; plenty of systems ignore it
        asio::error_code tos_ec;
        _udp_server->set_option(asio::detail::socket_option::integer<IPPROTO_IP, IP_TOS>(0xb8), tos_ec);
        asio::co_spawn(*_ioc, accept_udp_loop(), asio::detached);

        // start udp success
        spdlog::info("udp listen success on {}", endpoint);
    }

    _capture_queue.clear();
    _encode_stop = false;
    _encode_thread = std::thread([self = shared_from_this()] {
        self->encode_loop();
    });

    _net_thread = std::thread([self = shared_from_this()] {
        self->_ioc->run();
    });

    spdlog::info("server started");
}

void network_manager::stop_server()
{
    if (_ioc) {
        _ioc->stop();
    }
    _net_thread.join();
    _audio_manager->stop();
    _encode_stop = true;
    ++_capture_signal;
    _capture_signal.notify_one();
    _encode_thread.join();
    _playing_peer_list.clear();
    update_peer_counts();
    _udp_server = nullptr;
    _ioc = nullptr;
    spdlog::info("server stopped");
}

void network_manager::wait_server()
{
    _net_thread.join();
}

bool network_manager::is_running() const
{
    return _ioc != nullptr;
}

asio::awaitable<void> network_manager::read_loop(std::shared_ptr<tcp_socket> peer)
{
    uint32_t client_caps = 0;
    bool wire_opus = false;

    while (true) {
        cmd_t cmd = cmd_t::cmd_none;
        auto [ec, _] = co_await asio::async_read(*peer, asio::buffer(&cmd, sizeof(cmd)));
        if (ec) {
            close_session(peer);
            spdlog::trace("{} {}", __func__, ec);
            break;
        }

        spdlog::trace("cmd {}", (uint32_t)cmd);

        if (cmd == cmd_t::cmd_get_format || cmd == cmd_t::cmd_get_format_v2) {
            if (cmd == cmd_t::cmd_get_format_v2) {
                auto [caps_ec, __] = co_await asio::async_read(*peer, asio::buffer(&client_caps, sizeof(client_caps)));
                if (caps_ec) {
                    close_session(peer);
                    spdlog::trace("{} {}", __func__, caps_ec);
                    break;
                }
            } else {
                client_caps = 0; // old clients can't play Opus
            }
            auto format = get_format_binary_for(client_caps, wire_opus);
            auto size = (uint32_t)format.size();
            std::array<asio::const_buffer, 3> buffers = {
                asio::buffer(&cmd, sizeof(cmd)),
                asio::buffer(&size, sizeof(size)),
                asio::buffer(format),
            };
            auto [ec, _] = co_await asio::async_write(*peer, buffers);
            if (ec) {
                close_session(peer);
                spdlog::trace("{} {}", __func__, ec);
                break;
            }
        } else if (cmd == cmd_t::cmd_start_play) {
            int id = add_playing_peer(peer, wire_opus);
            if (id <= 0) {
                spdlog::error("{} id error", __func__);
                close_session(peer);
                spdlog::trace("{} {}", __func__, ec);
                break;
            }
            std::array<asio::const_buffer, 2> buffers = {
                asio::buffer(&cmd, sizeof(cmd)),
                asio::buffer(&id, sizeof(id)),
            };
            auto [ec, _] = co_await asio::async_write(*peer, buffers);
            if (ec) {
                spdlog::trace("{} {}", __func__, ec);
                close_session(peer);
                break;
            }
            asio::co_spawn(*_ioc, heartbeat_loop(peer), asio::detached);
        } else if (cmd == cmd_t::cmd_heartbeat) {
            auto it = _playing_peer_list.find(peer);
            if (it != _playing_peer_list.end()) {
                it->second->last_tick = std::chrono::steady_clock::now();
            }
        } else {
            spdlog::error("{} error cmd", __func__);
            close_session(peer);
            break;
        }
    }
    spdlog::trace("stop {}", __func__);
}

asio::awaitable<void> network_manager::heartbeat_loop(std::shared_ptr<tcp_socket> peer)
{
    std::error_code ec;
    size_t _;

    steady_timer timer(*_ioc);
    while (true) {
        timer.expires_after(3s);
        std::tie(ec) = co_await timer.async_wait();
        if (ec) {
            break;
        }

        if (!peer->is_open()) {
            break;
        }

        auto it = _playing_peer_list.find(peer);
        if (it == _playing_peer_list.end()) {
            spdlog::trace("{} it == _playing_peer_list.end()", __func__);
            close_session(peer);
            break;
        }
        if (std::chrono::steady_clock::now() - it->second->last_tick > _heartbeat_timeout) {
            spdlog::info("{} timeout", it->first->remote_endpoint());
            close_session(peer);
            break;
        }

        auto cmd = cmd_t::cmd_heartbeat;
        std::tie(ec, _) = co_await asio::async_write(*peer, asio::buffer(&cmd, sizeof(cmd)));
        if (ec) {
            spdlog::trace("{} {}", __func__, ec);
            close_session(peer);
            break;
        }
    }
    spdlog::trace("stop {}", __func__);
}

asio::awaitable<void> network_manager::accept_tcp_loop(tcp_acceptor acceptor)
{
    while (true) {
        auto peer = std::make_shared<tcp_socket>(acceptor.get_executor());
        auto [ec] = co_await acceptor.async_accept(*peer);
        if (ec) {
            spdlog::error("{} {}", __func__, ec);
            co_return;
        }

        spdlog::info("accept {}", peer->remote_endpoint());

        // No-Delay
        peer->set_option(ip::tcp::no_delay(true), ec);
        if (ec) {
            spdlog::info("{} {}", __func__, ec);
        }

        asio::co_spawn(acceptor.get_executor(), read_loop(peer), asio::detached);
    }
}

asio::awaitable<void> network_manager::accept_udp_loop()
{
    while (true) {
        int id = 0;
        ip::udp::endpoint udp_peer;
        auto [ec, _] = co_await _udp_server->async_receive_from(asio::buffer(&id, sizeof(id)), udp_peer);
        if (ec) {
            spdlog::info("{} {}", __func__, ec);
            co_return;
        }

        fill_udp_peer(id, udp_peer);
    }
}

auto network_manager::close_session(std::shared_ptr<tcp_socket>& peer) -> playing_peer_list_t::iterator
{
    spdlog::info("close {}", peer->remote_endpoint());
    auto it = remove_playing_peer(peer);
    peer->shutdown(ip::tcp::socket::shutdown_both);
    peer->close();
    return it;
}

// dual-stack sockets report IPv4 peers as v4-mapped v6 addresses
static asio::ip::address normalize_address(const asio::ip::address& a)
{
    if (a.is_v6() && a.to_v6().is_v4_mapped()) {
        return asio::ip::make_address_v4(asio::ip::v4_mapped, a.to_v6());
    }
    return a;
}

int network_manager::add_playing_peer(std::shared_ptr<tcp_socket>& peer, bool opus)
{
    if (_playing_peer_list.contains(peer)) {
        spdlog::error("{} repeat add tcp://{}", __func__, peer->remote_endpoint());
        return 0;
    }

    asio::error_code ec;
    auto remote = peer->remote_endpoint(ec);
    if (ec) {
        spdlog::error("{} no remote endpoint: {}", __func__, ec);
        return 0;
    }

    auto info = std::make_shared<peer_info_t>();
    info->opus = opus;
    info->tcp_address = normalize_address(remote.address());
    info->last_tick = std::chrono::steady_clock::now();

    // random so a LAN neighbour can't guess it; has to stay > 0 because clients reject id <= 0
    std::uniform_int_distribution<int> dist(1, std::numeric_limits<int>::max());
    do {
        info->id = dist(_id_rng);
    } while (std::any_of(_playing_peer_list.begin(), _playing_peer_list.end(), [&](const auto& e) { return e.second->id == info->id; }));

    _playing_peer_list[peer] = info;
    update_peer_counts();

    spdlog::trace("{} add id:{} opus:{} tcp://{}", __func__, info->id, opus, remote);
    return info->id;
}

void network_manager::update_peer_counts()
{
    int opus = 0, pcm = 0;
    for (auto& [_, info] : _playing_peer_list) {
        (info->opus ? opus : pcm)++;
    }
    _opus_peer_count = opus;
    _pcm_peer_count = pcm;
}

auto network_manager::remove_playing_peer(std::shared_ptr<tcp_socket>& peer) -> playing_peer_list_t::iterator
{
    auto it = _playing_peer_list.find(peer);
    if (it == _playing_peer_list.end()) {
        spdlog::error("{} repeat remove tcp://{}", __func__, peer->remote_endpoint());
        return it;
    }

    it = _playing_peer_list.erase(it);
    update_peer_counts();
    spdlog::trace("{} remove tcp://{}", __func__, peer->remote_endpoint());
    return it;
}

void network_manager::fill_udp_peer(int id, asio::ip::udp::endpoint udp_peer)
{
    auto it = std::find_if(_playing_peer_list.begin(), _playing_peer_list.end(), [id](const playing_peer_list_t::value_type& e) {
        return e.second->id == id;
    });

    if (it == _playing_peer_list.cend()) {
        spdlog::error("{} no tcp peer id:{} udp://{}", __func__, id, udp_peer);
        return;
    }

    // stops anyone who knows the id from redirecting the stream to another host
    if (normalize_address(udp_peer.address()) != it->second->tcp_address) {
        spdlog::warn("{} id:{} udp://{} does not match the tcp peer address {}, ignored", __func__, id, udp_peer, it->second->tcp_address.to_string());
        return;
    }

    it->second->udp_peer = udp_peer;
    spdlog::info("{} fill udp peer id:{} tcp://{} udp://{}", __func__, id, it->first->remote_endpoint(), udp_peer);
}

bool network_manager::ensure_codec()
{
    std::lock_guard lock(_codec_mutex);
    if (_codec_resolved) {
        return true;
    }

    auto format = _audio_manager->get_format();
    if (!format || format->channels() == 0) {
        return false; // capture format isn't known yet
    }

    _raw_format = *format;
    _opus_format = *format;
    _capture_encoding = format->encoding();

#ifdef AUDIO_SHARE_WITH_OPUS
    if (_requested_compression == audio_manager::compression_t::compression_opus) {
        std::string error;
        auto encoder = opus_stream_encoder::create(format->encoding(), format->channels(), format->sample_rate(), _requested_bitrate, error);
        if (encoder) {
            // the client decodes Opus to 16-bit PCM at the encoder's rate
            _opus_format.set_encoding(audio_manager::AudioFormat::ENCODING_PCM_16BIT);
            _opus_format.set_sample_rate(encoder->output_sample_rate());
            _opus_format.set_channels(encoder->channels());
            _opus_format.set_opus_pre_skip(encoder->pre_skip());
            _opus_format.set_compression(audio_manager::AudioFormat::COMPRESSION_OPUS);
            _opus_encoder = std::move(encoder);
            spdlog::info("opus compression enabled, bitrate: {} bps\nopus AudioFormat:\n{}", _requested_bitrate, _opus_format.DebugString());
        } else {
            spdlog::warn("opus compression disabled, sending uncompressed audio: {}", error);
        }
    }
#endif

    spdlog::info("raw AudioFormat:\n{}", _raw_format.DebugString());
    _codec_resolved = true;
    return true;
}

std::string network_manager::get_format_binary_for(uint32_t client_caps, bool& opus)
{
    opus = false;
    if (!ensure_codec()) {
        return _audio_manager->get_format_binary();
    }

#ifdef AUDIO_SHARE_WITH_OPUS
    if (_opus_encoder) {
        if (client_caps & cap_opus) {
            opus = true;
            return _opus_format.SerializeAsString();
        }
        spdlog::info("client doesn't support opus, sending raw PCM");
    }
#endif
    return _raw_format.SerializeAsString();
}

void network_manager::broadcast_audio_data(const char* data, size_t count, int block_align)
{
    if (count == 0 || _opus_peer_count + _pcm_peer_count == 0) {
        return;
    }

    _block_align = block_align;
    if (!_capture_queue.push((const uint8_t*)data, count)) {
        ++_dropped_chunks;
    }
    ++_capture_signal;
    _capture_signal.notify_one();
}

void network_manager::encode_loop()
{
    std::vector<uint8_t> chunk;
    size_t reported_drops = 0;

    while (!_encode_stop) {
        const uint32_t seen = _capture_signal.load();
        if (!_capture_queue.pop(chunk)) {
            _capture_signal.wait(seen);
            continue;
        }

        const size_t drops = _dropped_chunks;
        if (drops != reported_drops) {
            spdlog::warn("encoder is behind, dropped {} audio chunks", drops - reported_drops);
            reported_drops = drops;
        }

        process_audio(chunk.data(), chunk.size(), _block_align);
    }
}

void network_manager::process_audio(const uint8_t* data, size_t count, int block_align)
{
    if (!should_transmit((const char*)data, count)) {
        return;
    }

    segment_list_t pcm_segments, opus_segments;

    bool opus_active = false;
#ifdef AUDIO_SHARE_WITH_OPUS
    opus_active = ensure_codec() && _opus_encoder;
    if (opus_active && _opus_peer_count > 0) {
        const size_t errors_before = _opus_encoder->errors();
        _opus_encoder->encode(data, count, [&](const uint8_t* packet, size_t size) {
            auto seg = std::make_shared<std::vector<uint8_t>>(size + sizeof(uint16_t));
            (*seg)[0] = (uint8_t)(_opus_seq & 0xff);
            (*seg)[1] = (uint8_t)(_opus_seq >> 8);
            std::copy(packet, packet + size, seg->begin() + sizeof(uint16_t));
            ++_opus_seq;
            opus_segments.push_back(std::move(seg));
        });
        if (_opus_encoder->errors() != errors_before) {
            spdlog::warn("opus encoder failed on {} frames", _opus_encoder->errors() - errors_before);
        }
    }
#endif

    if (!opus_active || _pcm_peer_count > 0) {
        constexpr int mtu = 1492;
        int max_seg_size = mtu - 20 - 8;
        max_seg_size -= max_seg_size % block_align; // a sample can't be split across datagrams

        for (size_t begin_pos = 0; begin_pos < count;) {
            const size_t real_seg_size = std::min(count - begin_pos, (size_t)max_seg_size);
            pcm_segments.push_back(std::make_shared<std::vector<uint8_t>>(data + begin_pos, data + begin_pos + real_seg_size));
            begin_pos += real_seg_size;
        }
    }

    send_segments(std::move(pcm_segments), std::move(opus_segments));
}

void network_manager::send_segments(segment_list_t pcm_segments, segment_list_t opus_segments)
{
    if (pcm_segments.empty() && opus_segments.empty()) {
        return;
    }

    _ioc->post([pcm_segments = std::move(pcm_segments), opus_segments = std::move(opus_segments), self = shared_from_this()] {
        for (auto& [peer, info] : self->_playing_peer_list) {
            if (info->udp_peer.port() == 0) {
                continue;
            }
            for (const auto& seg : info->opus ? opus_segments : pcm_segments) {
                self->_udp_server->async_send_to(asio::buffer(*seg), info->udp_peer, [seg](const asio::error_code& ec, std::size_t bytes_transferred) { });
            }
        }
    });
}

bool network_manager::should_transmit(const char* data, size_t count)
{
    if (_silence_timeout.count() <= 0 || !ensure_codec()) {
        return true;
    }

    if (!silence::is_silent((const uint8_t*)data, count, _capture_encoding)) {
        if (_idle) {
            spdlog::info("audio detected, streaming resumed");
            _idle = false;
        }
        _silent_tracking = false;
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!_silent_tracking) {
        _silent_tracking = true;
        _silent_since = now;
    }
    if (_idle) {
        return false;
    }
    if (now - _silent_since >= _silence_timeout) {
        spdlog::info("no audio for {} ms, streaming paused until sound is played", _silence_timeout.count());
        _idle = true;
        return false;
    }
    return true; // short silence (pauses between tracks): keep the stream continuous
}
