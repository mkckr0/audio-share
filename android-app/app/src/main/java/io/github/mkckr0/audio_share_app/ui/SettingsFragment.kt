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

package io.github.mkckr0.audio_share_app.ui

import android.Manifest
import android.app.DownloadManager
import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.provider.MediaStore
import android.provider.MediaStore.DownloadColumns
import android.provider.OpenableColumns
import android.text.InputType
import android.util.Log
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.core.app.ActivityCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceManager
import androidx.preference.SwitchPreference
import com.google.android.material.color.DynamicColors
import com.google.android.material.snackbar.Snackbar
import io.github.mkckr0.audio_share_app.BuildConfig
import io.github.mkckr0.audio_share_app.MainActivity
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.Channels
import io.github.mkckr0.audio_share_app.model.LatestRelease
import io.github.mkckr0.audio_share_app.model.Notifications
import io.ktor.client.HttpClient
import io.ktor.client.call.body
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.client.request.get
import io.ktor.serialization.kotlinx.json.json
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonNamingStrategy
import java.io.File

class SettingsFragment : PreferenceFragmentCompat() {

    private val tag = javaClass.simpleName
    private lateinit var powerManager: PowerManager
    private lateinit var downloadManager: DownloadManager

    @OptIn(ExperimentalSerializationApi::class)
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {

        setPreferencesFromResource(R.xml.root_preferences, rootKey)

        powerManager = requireActivity().getSystemService(Context.POWER_SERVICE) as PowerManager

        findPreference<EditTextPreference>("audio_tcp_connect_timeout")!!.apply {
            setOnBindEditTextListener {
                it.inputType = InputType.TYPE_CLASS_NUMBER
                it.setSelection(it.length())
            }
            setSummaryProvider { "${(it as EditTextPreference).text}ms" }
            setOnPreferenceChangeListener { _, newValue ->
                if (newValue !is String || newValue.isEmpty()) {
                    return@setOnPreferenceChangeListener false
                }
                return@setOnPreferenceChangeListener newValue.toInt() in 1..10000
            }
        }
        findPreference<EditTextPreference>("audio_buffer_size_scale")!!.apply {
            setOnBindEditTextListener {
                it.inputType = InputType.TYPE_CLASS_NUMBER
                it.setSelection(it.length())
            }
            setSummaryProvider { "${(it as EditTextPreference).text}x" }
            setOnPreferenceChangeListener { _, newValue ->
                if (newValue !is String || newValue.isEmpty()) {
                    return@setOnPreferenceChangeListener false
                }
                return@setOnPreferenceChangeListener newValue.toInt() in 1..1000
            }
        }
        findPreference<EditTextPreference>("audio_loudness_enhancer")!!.apply {
            setOnBindEditTextListener {
                it.inputType = InputType.TYPE_CLASS_NUMBER
                it.setSelection(it.length())
            }
            setSummaryProvider { "${(it as EditTextPreference).text}mB" }
            setOnPreferenceChangeListener { _, newValue ->
                if (newValue !is String || newValue.isEmpty()) {
                    return@setOnPreferenceChangeListener false
                }
                val mB = newValue.toInt()
                if (mB > 1000) {
                    Snackbar.make(
                        requireView(),
                        "Too much loudness will hurt your ears!!!",
                        Snackbar.LENGTH_LONG
                    ).show()
                }
                return@setOnPreferenceChangeListener mB in 0..3000
            }
        }

        updateRequestIgnoreBatteryOptimizations()

        // update
        findPreference<Preference>("update_check")!!.apply {
            setOnPreferenceClickListener {
                MainScope().launch(Dispatchers.IO) {
                    val httpClient = HttpClient {
                        expectSuccess = true
                        install(ContentNegotiation) {
                            json(Json {
                                ignoreUnknownKeys = true
                                prettyPrint = true
                                isLenient = true
                                namingStrategy = JsonNamingStrategy.SnakeCase
                            })
                        }
                    }
                    val res =
                        httpClient.get("https://api.github.com/repos/mkckr0/audio-share/releases/latest")
                    val latestRelease: LatestRelease = res.body()

                    @Suppress("LABEL_NAME_CLASH") MainScope().launch(Dispatchers.Main) {
                        if (latestRelease.name <= "v${BuildConfig.VERSION_NAME}") {
                            Snackbar.make(requireView(), "No update", Snackbar.LENGTH_LONG).show()
                            return@launch
                        }

                        val apkAsset = latestRelease.assets.find {
                            it.name.matches(Regex("audio-share-app-[0-9.]*-release.apk"))
                        }
                        if (apkAsset == null) {
                            Snackbar.make(
                                requireView(), "Has an update, but no APK", Snackbar.LENGTH_LONG
                            ).show()
                            return@launch
                        }

                        with(NotificationManagerCompat.from(requireContext())) {
                            if (ActivityCompat.checkSelfPermission(
                                    requireContext(),
                                    Manifest.permission.POST_NOTIFICATIONS
                                ) != PackageManager.PERMISSION_GRANTED
                            ) {
                                Snackbar.make(
                                    requireView(),
                                    "Notification permission is denied",
                                    Snackbar.LENGTH_LONG
                                ).show()
                                return@with
                            }

                            val intent = Intent(requireActivity(), MainActivity::class.java).apply {
                                flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
                            }
                            intent.putExtra("action", "update")
                            intent.putExtra("apkAsset", apkAsset)
                            val pendingIntent = PendingIntent.getActivity(
                                requireActivity(),
                                0,
                                intent,
                                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
                            )

                            val notification = NotificationCompat.Builder(
                                requireContext(),
                                Channels.CHANNEL_UPDATE.id
                            )
                                .setSmallIcon(R.drawable.baseline_update)
                                .setContentTitle("Has an update(${latestRelease.tagName})")
                                .setContentText("Tap this notification to start updating")
                                .setContentIntent(pendingIntent)
                                .setAutoCancel(true)
                                .build()
                            notify(Notifications.NOTIFICATION_ID_UPDATE, notification)
                            Snackbar.make(
                                requireView(),
                                "Has an update, tap the notification to start updating",
                                Snackbar.LENGTH_LONG
                            ).show()
                        }
                    }
                }
                return@setOnPreferenceClickListener true
            }
        }

        findPreference<Preference>("version")!!.apply {
            summary =
                "v${BuildConfig.VERSION_NAME}(${BuildConfig.VERSION_CODE})-${BuildConfig.BUILD_TYPE}"
        }

        if (!DynamicColors.isDynamicColorAvailable()) {
            findPreference<SwitchPreference>("theme_use_wallpaper")!!.apply {
                isEnabled = false
                summary = getString(R.string.dynamic_color_is_not_available)
            }
            findPreference<EditTextPreference>("theme_color")!!.apply {
                isEnabled = false
            }
        } else {
            findPreference<EditTextPreference>("theme_color")!!.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    try {
                        Color.parseColor(newValue as String)
                        return@setOnPreferenceChangeListener true
                    } catch (e: IllegalArgumentException) {
                        Snackbar.make(
                            requireView(), "Color String is not valid", Snackbar.LENGTH_LONG
                        ).show()
                        return@setOnPreferenceChangeListener false
                    }
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
//        Log.d(tag, "onResume")
        val changed = updateRequestIgnoreBatteryOptimizations()
        if (changed) {
            return
        }

        // some devices may have delay of updating isIgnoringBatteryOptimizations
        Handler(Looper.getMainLooper()).postDelayed({
            updateRequestIgnoreBatteryOptimizations()
        }, 2000)
    }

    private fun updateRequestIgnoreBatteryOptimizations(): Boolean {
        val preference = findPreference<Preference>("request_ignore_battery_optimizations")!!
        val newSummary =
            if (powerManager.isIgnoringBatteryOptimizations(BuildConfig.APPLICATION_ID)) {
                "Ignored"
            } else {
                "Not Ignored"
            }
        val changed = newSummary != preference.summary
        preference.summary = newSummary
        return changed
    }
}