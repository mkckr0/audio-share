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

#include <set>
#include <map>
#include <vector>
#include <asio.hpp>
#include <mmreg.h>
#include <mmdeviceapi.h>

class audio_manager
{
public:

    struct peer_info_t {
        int id = 0;
        std::shared_ptr<asio::ip::tcp::socket> tcp_peer;
        asio::ip::udp::endpoint udp_peer;
    };

    audio_manager();
    ~audio_manager();

    asio::awaitable<void> do_loopback_recording(asio::io_context& ioc, const std::wstring& endpoint_id);

    int add_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer);
    void remove_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer);
    void fill_udp_peer(int id, asio::ip::udp::endpoint udp_peer);
    void init_udp_server(std::shared_ptr<asio::ip::udp::socket> udp_server);

    const std::vector<uint8_t>& get_format() const;

    static std::map<std::wstring, std::wstring> get_audio_endpoint_map();
    static std::wstring get_default_endpoint();

private:
    void set_format(WAVEFORMATEX* format);
    void broadcast_audio_data(const char* data, int count, int block_align);

    std::shared_ptr<asio::ip::udp::socket> _udp_server;
    std::map<std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<peer_info_t>> _playing_peer_list;
    std::vector<uint8_t> _format;

    constexpr static int _endpoint_state_mask = DEVICE_STATE_ACTIVE;
};

