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