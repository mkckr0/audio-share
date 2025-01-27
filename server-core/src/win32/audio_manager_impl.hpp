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

#ifndef AUDIO_MANAGER_HPP
#define AUDIO_MANAGER_HPP

#ifdef _WINDOWS

// fix: repeated inclusion of winsock2.h
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <vector>
#include <mutex>
#include <wil/com.h>
#include <Audioclient.h>

#include "client.pb.h"


class network_manager;

using AudioFormat_Encoding = io::github::mkckr0::audio_share_app::pb::AudioFormat_Encoding;

namespace detail {

class audio_manager_impl {
public:
    audio_manager_impl();
    ~audio_manager_impl();

    WORD nBlockAlign = 0;
    HANDLE hEvent = nullptr;
    UINT32 bufferFrameCount = 0;
    wil::com_ptr<IAudioClient> pAudioClient;
    wil::com_ptr<IAudioRenderClient> pRenderClient;
    
    std::thread _play_thread;
    std::atomic<bool> _running { false };

    size_t _buffer_capacity = 1024 * 1024;
    std::vector<char> _ring_buffer;
    std::atomic<size_t> _write_pos { 0 };
    std::atomic<size_t> _read_pos { 0 };
    std::mutex _buffer_mutex;
};

} // namespace detail

std::string wchars_to_mbs(const std::wstring& src);
std::string wchars_to_utf8(const std::wstring& src);
std::wstring mbs_to_wchars(const std::string& src);
std::string str_win_err(int err);
std::wstring wstr_win_err(int err);

#endif // _WINDOWS
#endif // !AUDIO_MANAGER_HPP