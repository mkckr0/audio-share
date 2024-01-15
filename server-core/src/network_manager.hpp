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

#ifndef _NETWORK_MANAGER_HPP
#define _NETWORK_MANAGER_HPP

#ifdef _WINDOWS
#include <sdkddkver.h>
#endif

#include <memory>
#include <vector>
#include <string>
#include <map>

#include <asio.hpp>
#include <asio/use_awaitable.hpp>

class audio_manager;

class network_manager : public std::enable_shared_from_this<network_manager>
{
    using default_token = asio::as_tuple_t<asio::use_awaitable_t<>>;
    using tcp_acceptor = default_token::as_default_on_t<asio::ip::tcp::acceptor>;
    using tcp_socket = default_token::as_default_on_t<asio::ip::tcp::socket>;
    using udp_socket = default_token::as_default_on_t<asio::ip::udp::socket>;
    using steady_timer = default_token::as_default_on_t<asio::steady_timer>;

    struct peer_info_t {
        int id = 0;
        asio::ip::udp::endpoint udp_peer;
        std::chrono::steady_clock::time_point last_tick;
    };

    using playing_peer_list_t = std::map<std::shared_ptr<tcp_socket>, std::shared_ptr<peer_info_t>>;

    enum class cmd_t : uint32_t {
        cmd_none = 0,
        cmd_get_format = 1,
        cmd_start_play = 2,
        cmd_heartbeat = 3,
    };

public:

    network_manager(std::shared_ptr<audio_manager>& audio_manager);

    static std::vector<std::wstring> get_local_addresss();

    void start_server(const std::string& host, const uint16_t port, const std::string& endpoint_id);
    void stop_server();
    void wait_server();

private:
    asio::awaitable<void> accept_tcp_loop(tcp_acceptor acceptor);
    asio::awaitable<void> read_loop(std::shared_ptr<tcp_socket> peer);
    asio::awaitable<void> heartbeat_loop(std::shared_ptr<tcp_socket> peer);
    asio::awaitable<void> accept_udp_loop();
    
    playing_peer_list_t::iterator close_session(std::shared_ptr<tcp_socket> peer);
    int add_playing_peer(std::shared_ptr<tcp_socket> peer);
    playing_peer_list_t::iterator remove_playing_peer(std::shared_ptr<tcp_socket> peer);
    void fill_udp_peer(int id, asio::ip::udp::endpoint udp_peer);

public:
    void broadcast_audio_data(const char* data, size_t count, int block_align);
    
    std::shared_ptr<asio::io_context> _ioc;

private:
    std::shared_ptr<audio_manager> _audio_manager;
    std::thread _net_thread;
    std::unique_ptr<udp_socket> _udp_server;
    playing_peer_list_t _playing_peer_list;
    constexpr static auto _heartbeat_timeout = std::chrono::seconds(5);
};

#endif // !_NETWORK_MANAGER_HPP