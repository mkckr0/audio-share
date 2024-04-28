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

import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.text.InputType
import android.util.Log
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import com.google.android.material.color.DynamicColors
import com.google.android.material.snackbar.Snackbar
import io.github.mkckr0.audio_share_app.BuildConfig
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.WorkName
import io.github.mkckr0.audio_share_app.worker.UpdateWorker
import java.util.concurrent.TimeUnit

class SettingsFragment : PreferenceFragmentCompat() {

    private val tag = javaClass.simpleName
    private lateinit var powerManager: PowerManager
    private lateinit var workManager: WorkManager

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {

        setPreferencesFromResource(R.xml.root_preferences, rootKey)

        powerManager = requireActivity().getSystemService(Context.POWER_SERVICE) as PowerManager
        workManager = WorkManager.getInstance(requireContext().applicationContext)

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
        findPreference<SwitchPreference>("update_auto_check")!!.apply {
            setOnPreferenceChangeListener { _, newValue ->
                val isChecked = newValue as Boolean
                if (isChecked) {
                    Log.d(tag, "beginAutoUpdate")
                    val updateWorker =
                        PeriodicWorkRequestBuilder<UpdateWorker>(
                            3, TimeUnit.HOURS,
                            300, TimeUnit.SECONDS,
                        ).build()
                    workManager.enqueueUniquePeriodicWork(
                        WorkName.AUTO_CHECK_UPDATE.value,
                        ExistingPeriodicWorkPolicy.CANCEL_AND_REENQUEUE,
                        updateWorker
                    )
                } else {
                    Log.d(tag, "stopAutoUpdate")
                    workManager.cancelUniqueWork(WorkName.AUTO_CHECK_UPDATE.value)
                }
                return@setOnPreferenceChangeListener true
            }
        }
        findPreference<Preference>("update_check")!!.apply {
            setOnPreferenceClickListener {
                val work = OneTimeWorkRequestBuilder<UpdateWorker>().build()
                workManager.enqueueUniqueWork(
                    WorkName.CHECK_UPDATE.value,
                    ExistingWorkPolicy.REPLACE,
                    work
                )
                return@setOnPreferenceClickListener true
            }
        }

        findPreference<Preference>("version")!!.apply {
            summary =
                "v${BuildConfig.VERSION_NAME}(${BuildConfig.VERSION_CODE})-${BuildConfig.BUILD_TYPE}"
            val url =
                "https://github.com/mkckr0/audio-share/releases/tag/v${BuildConfig.VERSION_NAME}"
            intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
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