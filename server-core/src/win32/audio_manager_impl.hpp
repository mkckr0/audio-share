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

#ifndef _AUDIO_MANAGER_HPP
#define _AUDIO_MANAGER_HPP

#ifdef _WINDOWS

#include <sdkddkver.h>
#include <string>

class network_manager;

namespace detail {

class audio_manager_impl {
public:
    audio_manager_impl();
    ~audio_manager_impl();
};

} // namespace detail

std::string wchars_to_mbs(const std::wstring& src);
std::string wchars_to_utf8(const std::wstring& src);
std::wstring mbs_to_wchars(const std::string& src);
std::string str_win_err(int err);
std::wstring wstr_win_err(int err);

#endif // _WINDOWS
#endif // !_AUDIO_MANAGER_HPP