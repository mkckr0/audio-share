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

package io.github.mkckr0.audio_share_app.ui.screen

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import android.util.Log
import android.widget.Toast
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Autorenew
import androidx.compose.material.icons.filled.BatterySaver
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.ColorLens
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.NewReleases
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.PlayCircle
import androidx.compose.material.icons.filled.Power
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Update
import androidx.compose.material.icons.filled.Wallpaper
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import io.github.mkckr0.audio_share_app.BuildConfig
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.AppSettingsKeys
import io.github.mkckr0.audio_share_app.model.WorkName
import io.github.mkckr0.audio_share_app.model.getBoolean
import io.github.mkckr0.audio_share_app.ui.base.EditTextPreference
import io.github.mkckr0.audio_share_app.ui.base.Preference
import io.github.mkckr0.audio_share_app.ui.base.PreferenceCategory
import io.github.mkckr0.audio_share_app.ui.base.PreferenceScreen
import io.github.mkckr0.audio_share_app.ui.base.SwitchPreference
import io.github.mkckr0.audio_share_app.ui.base.rememberIntent
import io.github.mkckr0.audio_share_app.ui.theme.AppTheme
import io.github.mkckr0.audio_share_app.ui.theme.isDynamicColorFromWallpaperAvailable
import io.github.mkckr0.audio_share_app.ui.theme.parseColor
import io.github.mkckr0.audio_share_app.worker.UpdateWorker
import java.util.concurrent.TimeUnit

@SuppressLint("BatteryLife")
@Composable
fun SettingsScreen() {

    val tag = "SettingsScreen"

    val context = LocalContext.current

    PreferenceScreen {

        PreferenceCategory("Auto Start") {
            SwitchPreference(
                icon = Icons.Default.PowerSettingsNew,
                key = AppSettingsKeys.START_PLAYBACK_WHEN_SYSTEM_BOOTED,
                title = "Auto start playback when system booted",
                defaultValue = context.getBoolean(R.bool.default_start_playback_when_system_booted)
            )
            SwitchPreference(
                icon = Icons.Default.PlayCircle,
                key = AppSettingsKeys.START_PLAYBACK_WHEN_APP_STARTED,
                title = "Auto start playback when app started",
                defaultValue = context.getBoolean(R.bool.default_start_playback_when_app_started)
            )
        }

        val getBatteryOptimizationState = {
            val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager
            if (powerManager.isIgnoringBatteryOptimizations(BuildConfig.APPLICATION_ID)) {
                "Ignored"
            } else {
                "Not ignored"
            }
        }
        var batteryOptimizationState by remember {
            mutableStateOf(getBatteryOptimizationState())
        }
        LifecycleEventEffect(Lifecycle.Event.ON_RESUME) {
            batteryOptimizationState = getBatteryOptimizationState()
        }

        PreferenceCategory("Battery optimization") {
            Preference(
                icon = Icons.Default.BatterySaver,
                title = "Request ignore battery optimization",
                summary = batteryOptimizationState,
                intent = rememberIntent(
                    Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                    "package:${BuildConfig.APPLICATION_ID}"
                )
            )
            Preference(
                icon = Icons.Default.Settings,
                title = "Battery optimization settings",
                summary = "Change battery optimization settings",
                intent = rememberIntent(
                    Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS
                )
            )
        }

        PreferenceCategory("Theme") {
            if (isDynamicColorFromWallpaperAvailable()) {
                SwitchPreference(
                    icon = Icons.Default.Wallpaper,
                    key = AppSettingsKeys.DYNAMIC_COLOR_FROM_WALLPAPER,
                    title = "Dynamic color from wallpaper",
                    defaultValue = context.getBoolean(R.bool.default_dynamic_color_from_wallpaper),
                )
            }
            EditTextPreference(
                icon = Icons.Default.ColorLens,
                key = AppSettingsKeys.DYNAMIC_COLOR_FROM_SEED_COLOR,
                title = "Dynamic color from seed color",
                defaultValue = context.getString(R.string.default_dynamic_color_from_seed_color),
            ) { newValue ->
                if (newValue.isBlank()) {
                    Toast.makeText(context, "color is invalid", Toast.LENGTH_SHORT).show()
                    return@EditTextPreference false
                }
                try {
                    Color.parseColor(newValue)
                } catch (_: IllegalArgumentException) {
                    Toast.makeText(context, "color is invalid", Toast.LENGTH_SHORT).show()
                    return@EditTextPreference false
                }
                return@EditTextPreference true
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            PreferenceCategory("Notification") {
                Preference(
                    icon = Icons.Default.Notifications,
                    title = "Notification",
                    summary = "Change notification settings",
                    intent = rememberIntent(
                        Settings.ACTION_APP_NOTIFICATION_SETTINGS,
                        extras = Bundle().apply {
                            putString(Settings.EXTRA_APP_PACKAGE, BuildConfig.APPLICATION_ID)
                        }
                    )
                )
            }
        }

        PreferenceCategory("Update") {
            val workManager = remember { WorkManager.getInstance(context.applicationContext) }
            SwitchPreference(
                icon = Icons.Default.Autorenew,
                key = AppSettingsKeys.AUTO_CHECK_FOR_UPDATE,
                title = "Auto check for update",
                defaultValue = context.getBoolean(R.bool.default_auto_check_for_update),
            ) { checked ->
                if (checked) {
                    Log.d(tag, "UpdateWorker beginAutoUpdate")
                    val updateWorker =
                        PeriodicWorkRequestBuilder<UpdateWorker>(
                            3, TimeUnit.HOURS,
                            5, TimeUnit.MINUTES,
//                            PeriodicWorkRequest.MIN_PERIODIC_INTERVAL_MILLIS, TimeUnit.MILLISECONDS,
//                            PeriodicWorkRequest.MIN_PERIODIC_FLEX_MILLIS, TimeUnit.MILLISECONDS,
                        ).build()
                    workManager.enqueueUniquePeriodicWork(
                        WorkName.AUTO_CHECK_UPDATE.value,
                        ExistingPeriodicWorkPolicy.CANCEL_AND_REENQUEUE,
                        updateWorker
                    )
                } else {
                    Log.d(tag, "UpdateWorker stopAutoUpdate")
                    workManager.cancelUniqueWork(WorkName.AUTO_CHECK_UPDATE.value)
                }
            }
            Preference(
                icon = Icons.Default.Update,
                title = "Check for update",
                summary = "Manually check for update",
            ) {
                val work = OneTimeWorkRequestBuilder<UpdateWorker>().build()
                workManager.enqueueUniqueWork(
                    WorkName.CHECK_UPDATE.value,
                    ExistingWorkPolicy.REPLACE,
                    work
                )
            }
            Preference(
                icon = Icons.Default.NewReleases,
                title = "Latest release",
                summary = "Access latest release on Github",
                intent = rememberIntent(
                    Intent.ACTION_VIEW,
                    stringResource(R.string.latest_release_url)
                )
            )
        }

        PreferenceCategory("About") {
            Preference(
                icon = R.drawable.github_mark,
                title = "Audio Share",
                summary = stringResource(R.string.project_url),
                intent = rememberIntent(
                    Intent.ACTION_VIEW,
                    stringResource(R.string.project_url)
                )
            )
            Preference(
                icon = Icons.Default.BugReport,
                title = "Issues",
                summary = "Report a bug or request a new feature",
                intent = rememberIntent(
                    Intent.ACTION_VIEW,
                    stringResource(R.string.issues_url)
                )
            )
            Preference(
                icon = Icons.Default.Info,
                title = "Version",
                summary = remember { "${BuildConfig.VERSION_NAME}(${BuildConfig.VERSION_CODE})-${BuildConfig.BUILD_TYPE}" },
                intent = rememberIntent(
                    Intent.ACTION_VIEW,
                    "https://github.com/mkckr0/audio-share/releases/tag/v${BuildConfig.VERSION_NAME}"
                ),
            )
        }
    }
}

@Preview
@Composable
fun SettingsScreenPreview() {
    AppTheme {
        SettingsScreen()
    }
}