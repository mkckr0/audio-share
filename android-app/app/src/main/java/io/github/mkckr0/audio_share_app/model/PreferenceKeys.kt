/*
 *    Copyright 2022-2024 mkckr0 <https://github.com/mkckr0>
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

package io.github.mkckr0.audio_share_app.model

object NetworkConfigKeys {
    const val HOST = "host"
    const val PORT = "port"
}

object AudioConfigKeys {
    const val VOLUME = "volume"
    const val BUFFER_SCALE = "buffer_scale"
    const val LOUDNESS_ENHANCER = "loudness_enhancer"
}

object AppSettingsKeys {
    const val START_PLAYBACK_WHEN_SYSTEM_BOOT = "start_playback_when_system_boot"
    const val START_PLAYBACK_WHEN_APP_START = "start_playback_when_app_start"
    const val DYNAMIC_COLOR_FROM_WALLPAPER = "dynamic_color_from_wallpaper"
    const val DYNAMIC_COLOR_FROM_SEED_COLOR = "dynamic_color_from_seed_color"
    const val AUTO_CHECK_FOR_UPDATE = "auto_check_for_update"
}