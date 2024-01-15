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

#ifndef _BASIC_AUDIO_MANAGER_HPP
#define _BASIC_AUDIO_MANAGER_HPP

#ifdef linux
#include "linux/audio_manager_impl.hpp"
#endif

#ifdef _WINDOWS
#include "win32/audio_manager_impl.hpp"
#endif

#include <asio.hpp>
#include <asio/use_awaitable.hpp>

#include <memory>

#include "client.pb.h"

class network_manager;

class audio_manager : private detail::audio_manager_impl, public std::enable_shared_from_this<audio_manager> {
public:
    using endpoint_list_t = std::vector<std::pair<std::string, std::string>>;
    using AudioFormat = io::github::mkckr0::audio_share_app::pb::AudioFormat;

    audio_manager();
    ~audio_manager();

    void start_loopback_recording(std::shared_ptr<network_manager> network_manager, const std::string& endpoint_id);
    void stop();
    void do_loopback_recording(std::shared_ptr<network_manager> network_manager, const std::string& endpoint_id);

    std::string get_format_binary();

    /// @brief Get audio endpoint list and the default endpoint index.
    /// @param endpoint_list Empty audio endpoint list.
    /// @return Default endpoint index in endpoint_list. Start from 0. If no default, return -1.
    int get_endpoint_list(endpoint_list_t& endpoint_list);

    std::string get_default_endpoint();
    
private:
    std::thread _record_thread;
    std::atomic_bool _stoppped;
    std::shared_ptr<AudioFormat> _format;
};

#endif // !_BASIC_AUDIO_MANAGER_HPP