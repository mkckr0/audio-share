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

#include <list>
#include <ranges>

#ifdef _WINDOWS
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif // _WINDOWS

#include <spdlog/spdlog.h>
#include <fmt/ranges.h>

namespace ip = asio::ip;
using namespace std::chrono_literals;

network_manager::network_manager(std::shared_ptr<audio_manager>& audio_manager)
    : _audio_manager(audio_manager)
{
}

std::vector<std::wstring> network_manager::get_local_addresss()
{
    std::vector<std::wstring> address_list;

#ifdef _WINDOWS
    ULONG family = AF_INET;
    ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;

    ULONG size = 0;
    GetAdaptersAddresses(family, flags, NULL, NULL, &size);
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(size);

    auto ret = GetAdaptersAddresses(family, flags, NULL, pAddresses, &size);
    if (ret == ERROR_SUCCESS) {
        for (auto pCurrentAddress = pAddresses; pCurrentAddress; pCurrentAddress = pCurrentAddress->Next) {
            if (pCurrentAddress->OperStatus != IfOperStatusUp || pCurrentAddress->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                continue;
            }

            for (auto pUnicast = pCurrentAddress->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
                auto sockaddr = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                wchar_t buf[20] {};
                if (InetNtopW(AF_INET, &sockaddr->sin_addr, buf, std::size(buf))) {
                    address_list.push_back(buf);
                }
            }
        }
    }

    free(pAddresses);
#endif // _WINDOWS

    std::sort(address_list.begin(), address_list.end(), [](const std::wstring& lhs, const std::wstring& rhs) {
        auto is_private_addr = [](const std::wstring& addr) {
            return addr.find(L"192.168.") == 0;
        };
        bool lhs_is_private = is_private_addr(lhs);
        bool rhs_is_private = is_private_addr(rhs);
        if (lhs_is_private != rhs_is_private) {
            return lhs_is_private > rhs_is_private;
        } else {
            return std::less<std::wstring>()(lhs, rhs);
        }
    });
    return address_list;
}

void network_manager::start_server(const std::string& host, const uint16_t port, const std::string& endpoint_id)
{
    _ioc = std::make_shared<asio::io_context>();
    {
        ip::tcp::endpoint endpoint { ip::make_address(host), port };

        ip::tcp::acceptor acceptor(*_ioc, endpoint.protocol());
        acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();

        _audio_manager->start_loopback_recording(shared_from_this(), endpoint_id);
        asio::co_spawn(*_ioc, accept_tcp_loop(std::move(acceptor)), asio::detached);

        // start tcp success
        spdlog::info("tcp listen success on {}", endpoint);
    }

    {
        ip::udp::endpoint endpoint { ip::make_address(host), port };
        _udp_server = std::make_unique<udp_socket>(*_ioc, endpoint.protocol());
        _udp_server->bind(endpoint);
        asio::co_spawn(*_ioc, accept_udp_loop(), asio::detached);

        // start udp success
        spdlog::info("udp listen success on {}", endpoint);
    }

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
    _playing_peer_list.clear();
    _udp_server = nullptr;
    _ioc = nullptr;
    spdlog::info("server stopped");
}

void network_manager::wait_server()
{
    _net_thread.join();
}

asio::awaitable<void> network_manager::read_loop(std::shared_ptr<tcp_socket> peer)
{
    while (true) {
        cmd_t cmd = cmd_t::cmd_none;
        auto [ec, _] = co_await asio::async_read(*peer, asio::buffer(&cmd, sizeof(cmd)));
        if (ec) {
            close_session(peer);
            spdlog::trace("{} {}", __func__, ec);
            break;
        }

        spdlog::trace("cmd {}", (uint32_t)cmd);

        if (cmd == cmd_t::cmd_get_format) {
            auto format = _audio_manager->get_format_binary();
            uint32_t size = (uint32_t)format.size();
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
            int id = add_playing_peer(peer);
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
    spdlog::info("stop {}", __func__);
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

auto network_manager::close_session(std::shared_ptr<tcp_socket> peer) -> playing_peer_list_t::iterator
{
    spdlog::info("close {}", peer->remote_endpoint());
    auto it = remove_playing_peer(peer);
    peer->shutdown(ip::tcp::socket::shutdown_both);
    peer->close();
    return it;
}

int network_manager::add_playing_peer(std::shared_ptr<tcp_socket> peer)
{
    if (_playing_peer_list.contains(peer)) {
        spdlog::error("{} repeat add tcp://{}", __func__, peer->remote_endpoint());
        return 0;
    }

    auto info = _playing_peer_list[peer] = std::make_shared<peer_info_t>();
    static int g_id = 0;
    info->id = ++g_id;
    info->last_tick = std::chrono::steady_clock::now();

    spdlog::trace("{} add id:{} tcp://{}", __func__, info->id, peer->remote_endpoint());
    return info->id;
}

auto network_manager::remove_playing_peer(std::shared_ptr<tcp_socket> peer) -> playing_peer_list_t::iterator
{
    auto it = _playing_peer_list.find(peer);
    if (it == _playing_peer_list.end()) {
        spdlog::error("{} repeat remove tcp://{}", __func__, peer->remote_endpoint());
        return it;
    }

    it = _playing_peer_list.erase(it);
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

    it->second->udp_peer = udp_peer;
    spdlog::trace("{} fill udp peer id:{} tcp://{} udp://{}", __func__, id, it->first->remote_endpoint(), udp_peer);
}

void network_manager::broadcast_audio_data(const char* data, size_t count, int block_align)
{
    if (count <= 0) {
        return;
    }
    //spdlog::trace("broadcast_audio_data count: {}", count);

    // divide udp frame
    constexpr int mtu = 1492;
    int max_seg_size = mtu - 20 - 8;
    max_seg_size -= max_seg_size % block_align; // one single sample can't be divided

    std::list<std::shared_ptr<std::vector<uint8_t>>> seg_list;

    for (int begin_pos = 0; begin_pos < count;) {
        const int real_seg_size = std::min((int)count - begin_pos, max_seg_size);
        auto seg = std::make_shared<std::vector<uint8_t>>(real_seg_size);
        std::copy((const uint8_t*)data + begin_pos, (const uint8_t*)data + begin_pos + real_seg_size, seg->begin());
        seg_list.push_back(seg);
        begin_pos += real_seg_size;
    }

    _ioc->post([seg_list = std::move(seg_list), self = shared_from_this()] {
        for (auto seg : seg_list) {
            for (auto& [peer, info] : self->_playing_peer_list) {
                self->_udp_server->async_send_to(asio::buffer(*seg), info->udp_peer, [seg](const asio::error_code& ec, std::size_t bytes_transferred) {});
            }
        }
    });
}
