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

#pragma once

#include <sdkddkver.h>

#include <memory>
#include <vector>
#include <string>
#include <asio.hpp>

class audio_manager;

class network_manager
{
    std::unique_ptr<class audio_manager> audio_manager;

public:

    network_manager() {}

    static std::vector<std::wstring> get_local_addresss();

    asio::awaitable<void> read_loop(std::shared_ptr<asio::ip::tcp::socket> peer);

    asio::awaitable<void> accept_tcp_loop(asio::ip::tcp::acceptor acceptor);
    
    asio::awaitable<void> accept_udp_loop(std::shared_ptr<asio::ip::udp::socket> acceptor);

    void start_server(const std::string& host, const uint16_t port, const std::wstring& endpoint_id);

    void stop_server();

private:
    std::shared_ptr<asio::io_context> ioc;
    std::thread net_thread;
};