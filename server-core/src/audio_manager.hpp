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
#include <asio/use_awaitable.hpp>

#include <mmreg.h>
#include <mmdeviceapi.h>

class network_manager;

class audio_manager
{
    using steady_timer = asio::as_tuple_t<asio::use_awaitable_t<>>::as_default_on_t<asio::steady_timer>;

    std::shared_ptr<network_manager> _network_manager;

public:
    audio_manager(std::shared_ptr<network_manager> network_manager);
    ~audio_manager();

    asio::awaitable<void> do_loopback_recording(asio::io_context& ioc, const std::wstring& endpoint_id);

    const std::vector<uint8_t>& get_format() const;

    static std::map<std::wstring, std::wstring> get_audio_endpoint_map();
    static std::wstring get_default_endpoint();

private:
    void set_format(WAVEFORMATEX* format);

    std::vector<uint8_t> _format;
};

