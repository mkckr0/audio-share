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

#ifdef linux

#include "audio_manager.hpp"
#include "client.pb.h"
#include "network_manager.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <vector>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spdlog/spdlog.h>

using namespace io::github::mkckr0::audio_share_app::pb;

struct roundtrip {
    struct pw_core* _core;
    int _sync;
    struct pw_main_loop* _loop;

    void operator()()
    {
        static const struct pw_core_events core_events = {
            .version = PW_VERSION_CORE_EVENTS,
            .done = [](void* data, uint32_t id, int seq) {
                auto* d = (struct roundtrip*)data;
                if (id == PW_ID_CORE && seq == d->_sync)
                    pw_main_loop_quit(d->_loop);
            },
        };

        struct spa_hook core_listener {};

        pw_core_add_listener(_core, &core_listener, &core_events, this);
        spdlog::trace("sync before: {}", _sync);
        _sync = pw_core_sync(_core, PW_ID_CORE, _sync);
        spdlog::trace("sync after: {}", _sync);
        pw_main_loop_run(_loop);

        spa_hook_remove(&core_listener);
    }
};

namespace detail {

audio_manager_impl::audio_manager_impl()
{
    pw_init(nullptr, nullptr);
    spdlog::info("pipewire header_version: {}, library_version: {}", pw_get_headers_version(), pw_get_library_version());

    _loop = pw_main_loop_new(nullptr);
    _context = pw_context_new(pw_main_loop_get_loop(_loop), nullptr, 0);
    _core = pw_context_connect(_context, nullptr, 0);
    _roundtrip = new roundtrip {
        ._core = _core,
        ._sync = 0,
        ._loop = _loop,
    };
}

audio_manager_impl::~audio_manager_impl()
{
    pw_core_disconnect(_core);
    pw_context_destroy(_context);
    pw_main_loop_destroy(_loop);
    pw_deinit();
    delete _roundtrip;
}

} // namespace detail

void audio_manager::do_loopback_recording(std::shared_ptr<network_manager> network_manager, const capture_config& config)
{
    spdlog::info("endpoint_id: {}", config.endpoint_id);
    std::string selected_endpoint_id = config.endpoint_id;
    if (selected_endpoint_id.empty() || selected_endpoint_id == "default") {
        selected_endpoint_id = get_default_endpoint();
    }

    endpoint_list_t endpoint_list;
    get_endpoint_list(endpoint_list);
    auto it = std::find_if(endpoint_list.begin(), endpoint_list.end(), [&](const endpoint_list_t::value_type& e) {
        return e.first == selected_endpoint_id;
    });
    if (it != endpoint_list.end()) {
        spdlog::info("select audio endpoint: {}", it->second);
    } else {
        spdlog::error("selected audio endpoint is not in list");
        return;
    }

    // set capture format
    auto spa_format = SPA_AUDIO_FORMAT_UNKNOWN;
    switch (config.encoding) {
    case encoding_t::encoding_default:
    case encoding_t::encoding_f32:
        spa_format = SPA_AUDIO_FORMAT_F32_LE;
        break;
    case encoding_t::encoding_s8:
        spa_format = SPA_AUDIO_FORMAT_U8;
        break;
    case encoding_t::encoding_s16:
        spa_format = SPA_AUDIO_FORMAT_S16_LE;
        break;
    case encoding_t::encoding_s24:
        spa_format = SPA_AUDIO_FORMAT_S24_LE;
        break;
    case encoding_t::encoding_s32:
        spa_format = SPA_AUDIO_FORMAT_S32_LE;
        break;
    default:
        spa_format = SPA_AUDIO_FORMAT_UNKNOWN;
        break;
    }
    uint32_t spa_channels = 0;
    if (config.channels) {
        spa_channels = config.channels;
    }
    uint32_t spa_sample_rate = 0;
    if (config.sample_rate) {
        spa_sample_rate = config.sample_rate;
    }

    struct user_data_t {
        struct pw_main_loop* loop;
        struct pw_stream* stream;
        std::shared_ptr<class network_manager> network_manager;
        std::shared_ptr<AudioFormat> format;
        int block_align;
    } user_data = {
        .loop = _loop,
        .stream = nullptr,
        .network_manager = network_manager,
        .format = _format,
        .block_align = 0,
    };

    static const struct pw_stream_events stream_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = [](void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error) {
            if (state == PW_STREAM_STATE_STREAMING) {
                if (spdlog::get_level() == spdlog::level::trace) {
                    auto user_data = (struct user_data_t*)data;
                    auto loop = pw_main_loop_get_loop(user_data->loop);
                    auto timer = pw_loop_add_timer(loop, [](void *data, uint64_t expirations){
                        auto user_data = (struct user_data_t*)data;
                        struct pw_time time{};
                        pw_stream_get_time_n(user_data->stream, &time, sizeof(time));
                        spdlog::trace("now:{} rate:{}/{} ticks:{} delay:{} queued:{}",
                            time.now,
                            time.rate.num, time.rate.denom,
                            time.ticks, time.delay, time.queued);
                    }, data);
                    struct timespec timeout = {0, 1}, interval = {1, 0};
                    pw_loop_update_timer(loop, timer, &timeout, &interval, false);
                }
            } },
        .param_changed = [](void* data, uint32_t id, const struct spa_pod* param) {
            auto* user_data = (struct user_data_t*)data;

            if (param == nullptr || id != SPA_PARAM_Format) {
                return;
            }

            struct spa_audio_info audio_info {};

            if (spa_format_parse(param, &audio_info.media_type, &audio_info.media_subtype) < 0) {
                return;
            }

            if (audio_info.media_type != SPA_MEDIA_TYPE_audio || audio_info.media_subtype != SPA_MEDIA_SUBTYPE_raw) {
                return;
            }

            spa_format_audio_raw_parse(param, &audio_info.info.raw);
            spdlog::info("audio_info.info.raw.format: {}", (int)audio_info.info.raw.format);

            switch (audio_info.info.raw.format)
            {
            case SPA_AUDIO_FORMAT_F32_LE:
                user_data->format->set_encoding(AudioFormat_Encoding_ENCODING_PCM_FLOAT);
                break;
            case SPA_AUDIO_FORMAT_S8:
                user_data->format->set_encoding(AudioFormat_Encoding_ENCODING_PCM_8BIT);
                break;
            case SPA_AUDIO_FORMAT_S16_LE:
                user_data->format->set_encoding(AudioFormat_Encoding_ENCODING_PCM_16BIT);
                break;
            case SPA_AUDIO_FORMAT_S24_LE:
                user_data->format->set_encoding(AudioFormat_Encoding_ENCODING_PCM_24BIT);
                break;
            case SPA_AUDIO_FORMAT_S32_LE:
                user_data->format->set_encoding(AudioFormat_Encoding_ENCODING_PCM_32BIT);
                break;
            default:
                user_data->format->set_encoding(AudioFormat_Encoding_ENCODING_INVALID);
                spdlog::info("the capture format is not supported");
                exit(EXIT_FAILURE);
            }
            spdlog::info("the capture format is supported");
            user_data->format->set_channels((int)audio_info.info.raw.channels);
            user_data->format->set_sample_rate((int)audio_info.info.raw.rate);
            int bits_per_sample = 0;
            switch (audio_info.info.raw.format)
            {
            case SPA_AUDIO_FORMAT_S8:
            case SPA_AUDIO_FORMAT_U8:
                bits_per_sample = 8;
                break;
            case SPA_AUDIO_FORMAT_S16_LE:
            case SPA_AUDIO_FORMAT_S16_BE:
            case SPA_AUDIO_FORMAT_U16_LE:
            case SPA_AUDIO_FORMAT_U16_BE:
                bits_per_sample = 16;
                break;
            case SPA_AUDIO_FORMAT_S24_LE:
            case SPA_AUDIO_FORMAT_S24_BE:
            case SPA_AUDIO_FORMAT_U24_LE:
            case SPA_AUDIO_FORMAT_U24_BE:
                bits_per_sample = 24;
                break;
            case SPA_AUDIO_FORMAT_S32_LE:
            case SPA_AUDIO_FORMAT_S32_BE:
            case SPA_AUDIO_FORMAT_U32_LE:
            case SPA_AUDIO_FORMAT_U32_BE:
            case SPA_AUDIO_FORMAT_F32_LE:
            case SPA_AUDIO_FORMAT_F32_BE:
            case SPA_AUDIO_FORMAT_F32P:
                bits_per_sample = 32;
                break;
            default:
                bits_per_sample = 0;
                break;
            }

            user_data->block_align = bits_per_sample / 8 * user_data->format->channels();
            spdlog::info("block_align: {}", user_data->block_align);
            spdlog::info("AudioFormat:\n{}", user_data->format->DebugString());
        },
        .process = [](void* data) {
            auto* user_data = (struct user_data_t*)data;
            struct pw_buffer *b;
            struct spa_buffer *buf;
    
            if ((b = pw_stream_dequeue_buffer(user_data->stream)) == nullptr) {
                pw_log_warn("out of buffers: %m");
                return;
            }
    
            buf = b->buffer;
            if (buf->datas[0].data == nullptr) {
                return;
            }

            auto begin = (const char*)buf->datas[0].data + buf->datas[0].chunk->offset;
            auto count = buf->datas[0].chunk->size;

            user_data->network_manager->broadcast_audio_data(begin, count, user_data->block_align);
    
            pw_stream_queue_buffer(user_data->stream, b); },
    };

    struct pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        nullptr);

    user_data.stream = pw_stream_new_simple(pw_main_loop_get_loop(_loop), "audio-share-server", props, &stream_events, &user_data);

    uint8_t buffer[1024];
    struct spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod* params[1];
    struct spa_audio_info_raw info = SPA_AUDIO_INFO_RAW_INIT(
            .format = spa_format,
        .rate = spa_sample_rate,
        .channels = spa_channels,
    );
    params[0] = spa_format_audio_raw_build(&pod_builder, SPA_PARAM_EnumFormat, &info);

    pw_stream_connect(user_data.stream, PW_DIRECTION_INPUT, std::stoi(selected_endpoint_id),
        pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT
            | PW_STREAM_FLAG_MAP_BUFFERS
            | PW_STREAM_FLAG_RT_PROCESS),
        params, 1);

    pw_main_loop_run(_loop);

    pw_stream_destroy(user_data.stream);
}

int audio_manager::get_endpoint_list(endpoint_list_t& endpoint_list)
{
    struct user_data_t {
        endpoint_list_t* endpoint_list_ptr;
        int default_index;
        int default_priority;
    };

    static const struct pw_registry_events registry_events = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = [](void* object, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props) {
            if (spa_streq(type, PW_TYPE_INTERFACE_Node)) {
                const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);

                if (!spa_streq(media_class, "Audio/Sink")) {
                    return;
                }

                auto user_data = (user_data_t*)object;
                // const char* object_serial = spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL);
                const char* nick_name = spa_dict_lookup(props, PW_KEY_NODE_NICK);
                const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
                const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
                const char* priority_session = spa_dict_lookup(props, PW_KEY_PRIORITY_SESSION);

                user_data->endpoint_list_ptr->push_back(std::make_pair<std::string, std::string>(
                    std::to_string(id),
                    nick_name ? nick_name : (description ? description : name)));

                int priority = priority_session ? std::stoi(priority_session) : -1;
                if (priority > user_data->default_priority) {
                    user_data->default_priority = priority;
                    user_data->default_index = (int)user_data->endpoint_list_ptr->size() - 1;
                }

                spdlog::trace("object id: {}", id);
                const struct spa_dict_item* item;
                spa_dict_for_each(item, props)
                {
                    spdlog::trace("\t{}: \"{}\"", item->key, item->value);
                }
            }
        },
    };

    struct pw_registry* registry = pw_core_get_registry(_core, PW_VERSION_REGISTRY, 0);
    struct spa_hook registry_listener {};
    struct user_data_t user_data = {
        .endpoint_list_ptr = &endpoint_list,
        .default_index = -1,
        .default_priority = -1,
    };
    pw_registry_add_listener(registry, &registry_listener, &registry_events, &user_data);
    (*_roundtrip)();

    pw_proxy_destroy((struct pw_proxy*)registry);
    return user_data.default_index;
}

std::string audio_manager::get_default_endpoint()
{
    endpoint_list_t endpoint_list;
    int index = get_endpoint_list(endpoint_list);
    return index >= 0 ? endpoint_list[index].first : "";
}

#endif // linux