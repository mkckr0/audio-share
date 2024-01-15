/**
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

package io.github.mkckr0.audio_share_app

import android.app.Application
import android.graphics.Color
import androidx.preference.PreferenceManager
import com.google.android.material.color.DynamicColors
import com.google.android.material.color.DynamicColorsOptions

class App : Application() {
    override fun onCreate() {
        super.onCreate()

        if (!DynamicColors.isDynamicColorAvailable()) {
            return
        }

        val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)

        val themeUseWallpaper = sharedPreferences.getBoolean("theme_use_wallpaper", resources.getBoolean(R.bool.theme_use_wallpaper_default))
        if (themeUseWallpaper) {
            DynamicColors.applyToActivitiesIfAvailable(this)
            return
        }

        val themeColor = sharedPreferences.getString("theme_color", resources.getString(R.string.theme_color_default))
        DynamicColors.applyToActivitiesIfAvailable(this, DynamicColorsOptions.Builder().setContentBasedSource(Color.parseColor(themeColor)).build())
    }
}