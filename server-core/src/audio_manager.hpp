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

#ifndef BASIC_AUDIO_MANAGER_HPP
#define BASIC_AUDIO_MANAGER_HPP

#ifdef linux
#include "linux/audio_manager_impl.hpp"
#endif

#ifdef _WINDOWS
#include "win32/audio_manager_impl.hpp"
#endif

#include <memory>
#include <sstream>

#include "client.pb.h"

class network_manager;

class audio_manager : private detail::audio_manager_impl, public std::enable_shared_from_this<audio_manager> {
public:
    using endpoint_list_t = std::vector<std::pair<std::string, std::string>>;
    using AudioFormat = io::github::mkckr0::audio_share_app::pb::AudioFormat;

    enum class encoding {
        encoding_default = 0,
        encoding_invalid = 1,
        encoding_f32 = 2,
        encoding_u8 = 3,
        encoding_u16 = 4,
        encoding_u24 = 5,
        encoding_u32 = 6,
    };

    friend std::istream& operator>>(std::istream& is, encoding& e) {
        std::string s;
        is >> s;
        if (s == "f32") {
            e = encoding::encoding_f32;
        } else if (s == "u8") {
            e = encoding::encoding_u8;
        } else if (s == "u16") {
            e = encoding::encoding_u16;
        } else if (s == "u24") {
            e = encoding::encoding_u24;
        } else if (s == "u32") {
            e = encoding::encoding_u32;
        } else {
            e = encoding::encoding_invalid;
        }
        return is;
    }

    struct capture_config {
        std::string endpoint_id;
        encoding encoding = encoding::encoding_default;
        int channels = 0;
        int sample_rate = 0;
    };

    audio_manager();
    ~audio_manager();

    void start_loopback_recording(std::shared_ptr<network_manager> network_manager, const capture_config& config);
    void stop();
    void do_loopback_recording(std::shared_ptr<network_manager> network_manager, const capture_config& config);

    std::string get_format_binary();

    /// @brief Get audio endpoint list and the default endpoint index.
    /// @param endpoint_list Empty audio endpoint list.
    /// @return Default endpoint index in endpoint_list. Start from 0. If no default, return -1.
    static int get_endpoint_list(endpoint_list_t& endpoint_list);

    static std::string get_default_endpoint();
    
private:
    std::thread _record_thread;
    std::atomic_bool _stopped;
    std::unique_ptr<AudioFormat> _format;
};

#endif // !BASIC_AUDIO_MANAGER_HPP