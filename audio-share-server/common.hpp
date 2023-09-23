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
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

constexpr auto co_token = asio::as_tuple(asio::use_awaitable);

template<> struct fmt::formatter<asio::ip::tcp::endpoint> : fmt::ostream_formatter {};
template<> struct fmt::formatter<asio::ip::udp::endpoint> : fmt::ostream_formatter {};

enum class cmd_t : uint32_t {
    cmd_none = 0,
    cmd_get_format = 1,
    cmd_start_play = 2,
};

std::string wchars_to_mbs(const wchar_t* src);

std::string str_win_err(int err);

std::wstring wstr_win_err(int err);