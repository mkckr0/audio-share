/*
   Copyright 2022-2023 mkckr0 <https://github.com/mkckr0>

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
#include "common.hpp"
#include "audio_manager.hpp"

#include <iostream>

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#include <asio/experimental/as_single.hpp>

namespace ip = asio::ip;

std::vector<std::wstring> network_manager::get_local_addresss()
{
    std::vector<std::wstring> address_list;

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
                wchar_t buf[20]{};
                if (InetNtop(AF_INET, &sockaddr->sin_addr, buf, std::size(buf))) {
                    address_list.push_back(buf);
                }
            }
        }
    }

    free(pAddresses);

    std::sort(address_list.begin(), address_list.end());
    return address_list;
}

void network_manager::start_server(const std::string& host, const uint16_t port, const std::wstring& endpoint_id)
{
    _ioc = std::make_shared<asio::io_context>();

    std::promise<void> promise;
    auto future = promise.get_future();
    _net_thread = std::thread([=, promise = std::move(promise)]() mutable {

        try {

            {
                ip::tcp::endpoint endpoint{ ip::make_address(host), port };

                ip::tcp::acceptor acceptor(*_ioc, endpoint.protocol());
                acceptor.bind(endpoint);

                audio_manager = std::make_unique<class audio_manager>(shared_from_this());
                asio::co_spawn(*_ioc, audio_manager->do_loopback_recording(*_ioc, endpoint_id), asio::detached);
                _ioc->run_one();

                acceptor.listen();

                asio::co_spawn(*_ioc, accept_tcp_loop(std::move(acceptor)), asio::detached);

                // start tcp success
                spdlog::info("tcp listen success on {}", endpoint);
            }

            {
                ip::udp::endpoint endpoint{ ip::make_address(host), port };
                _udp_server = std::make_unique<udp_socket>(*_ioc, endpoint.protocol());
                _udp_server->bind(endpoint);
                asio::co_spawn(*_ioc, accept_udp_loop(), asio::detached);

                // start udp success
                spdlog::info("udp listen success on {}", endpoint);
            }

            promise.set_value();
        }
        catch (...) {
            promise.set_exception(std::current_exception());
            return;
        }

        _ioc->run();
        });

    try {
        future.get();
    }
    catch (...) {
        stop_server();
        throw;
    }
}

void network_manager::stop_server()
{
    if (_ioc) {
        _ioc->stop();
    }
    _net_thread.join();
    _udp_server = nullptr;
    audio_manager = nullptr;
    _ioc = nullptr;
    _playing_peer_list.clear();
}

asio::awaitable<void> network_manager::read_loop(std::shared_ptr<tcp_socket> peer)
{
    while (true) {
        cmd_t cmd = cmd_t::cmd_none;
        auto [ec, _] = co_await asio::async_read(*peer, asio::buffer(&cmd, sizeof(cmd)));
        if (ec) {
            remove_playing_peer(peer);
            peer->shutdown(ip::tcp::socket::shutdown_both);
            spdlog::info("{} {}", __func__, ec.message());
            co_return;
        }

        spdlog::info("cmd {}", (uint32_t)cmd);

        if (cmd == cmd_t::cmd_get_format) {
            auto& format = audio_manager->get_format();
            uint32_t size = (uint32_t)format.size();
            std::array<asio::const_buffer, 3> buffers = {
                asio::buffer(&cmd, sizeof(cmd)),
                asio::buffer(&size, sizeof(size)),
                asio::buffer(format),
            };
            co_await asio::async_write(*peer, buffers);
        }
        else if (cmd == cmd_t::cmd_start_play) {
            int id = add_playing_peer(peer);
            if (id <= 0) {
                spdlog::error("{} id error", __func__);
                peer->shutdown(ip::tcp::socket::shutdown_both);
                continue;
            }
            std::array<asio::const_buffer, 2> buffers = {
                asio::buffer(&cmd, sizeof(cmd)),
                asio::buffer(&id, sizeof(id)),
            };
            co_await asio::async_write(*peer, buffers);
        }
        else {
            spdlog::error("{} error cmd", __func__);
            remove_playing_peer(peer);
            peer->shutdown(ip::tcp::socket::shutdown_both);
            co_return;
        }
    }
}

asio::awaitable<void> network_manager::accept_tcp_loop(tcp_acceptor acceptor)
{
    while (true) {
        auto peer = std::make_shared<tcp_socket>(acceptor.get_executor());
        auto [ec] = co_await acceptor.async_accept(*peer);
        if (ec) {
            spdlog::error("{} {}", __func__, ec.message());
            co_return;
        }
        spdlog::info("{} {}", __func__, peer->remote_endpoint());
        peer->set_option(ip::tcp::socket::keep_alive(true), ec);
        if (ec) {
            spdlog::info("{} {}", __func__, ec.message());
        }
        peer->set_option(ip::tcp::no_delay(true), ec);
        if (ec) {
            spdlog::info("{} {}", __func__, ec.message());
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
            spdlog::info("{} {}", __func__, ec.message());
            co_return;
        }

        fill_udp_peer(id, udp_peer);
    }
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
    info->tcp_peer = peer;

    spdlog::info("{} add id:{} tcp://{}", __func__, info->id, peer->remote_endpoint());
    return info->id;
}

void network_manager::remove_playing_peer(std::shared_ptr<tcp_socket> peer)
{
    if (!_playing_peer_list.contains(peer)) {
        spdlog::error("{} repeat remove tcp://{}", __func__, peer->remote_endpoint());
        return;
    }

    _playing_peer_list.erase(peer);
    spdlog::info("{} remove tcp://{}", __func__, peer->remote_endpoint());
}

void network_manager::fill_udp_peer(int id, asio::ip::udp::endpoint udp_peer)
{
    auto it = std::find_if(_playing_peer_list.begin(), _playing_peer_list.end(), [id](const std::pair<std::shared_ptr<tcp_socket>, std::shared_ptr<peer_info_t>>& e) {
        return e.second->id == id;
        });

    if (it == _playing_peer_list.cend()) {
        spdlog::error("{} no tcp peer id:{} udp://{}", __func__, id, udp_peer);
        return;
    }

    it->second->udp_peer = udp_peer;
    spdlog::info("{} fill udp peer id:{} tcp://{} udp://{}", __func__, id, it->second->tcp_peer->remote_endpoint(), udp_peer);
}

void network_manager::broadcast_audio_data(const char* data, int count, int block_align)
{
    if (count <= 0) {
        return;
    }

    //spdlog::info("size: {}", count);

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

    for (auto seg : seg_list) {
        for (auto& [peer, info] : _playing_peer_list) {
            _udp_server->async_send_to(asio::buffer(*seg), info->udp_peer, [seg](const asio::error_code& ec, std::size_t bytes_transferred) {});
        }
    }
}
