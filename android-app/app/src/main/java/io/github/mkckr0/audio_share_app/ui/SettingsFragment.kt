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
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.text.InputType
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import com.google.android.material.color.DynamicColors
import com.google.android.material.snackbar.Snackbar
import io.github.mkckr0.audio_share_app.BuildConfig
import io.github.mkckr0.audio_share_app.R

class SettingsFragment : PreferenceFragmentCompat() {

    private val tag = javaClass.simpleName
    private lateinit var powerManager: PowerManager

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
                if (newValue is String) {
                    if (newValue.isEmpty()) {
                        return@setOnPreferenceChangeListener false
                    }
                    return@setOnPreferenceChangeListener newValue.toInt() <= 10000
                }
                return@setOnPreferenceChangeListener false
            }
        }
        findPreference<EditTextPreference>("audio_buffer_size_scale")!!.apply {
            setOnBindEditTextListener {
                it.inputType = InputType.TYPE_CLASS_NUMBER
                it.setSelection(it.length())
            }
            setSummaryProvider { "${(it as EditTextPreference).text}x" }
            setOnPreferenceChangeListener { _, newValue ->
                if (newValue is String) {
                    if (newValue.isEmpty()) {
                        return@setOnPreferenceChangeListener false
                    }
                    return@setOnPreferenceChangeListener newValue.toInt() <= 1000
                }
                return@setOnPreferenceChangeListener false
            }
        }
        findPreference<EditTextPreference>("audio_loudness_enhancer")!!.apply {
            setOnBindEditTextListener {
                it.inputType = InputType.TYPE_CLASS_NUMBER
                it.setSelection(it.length())
            }
            setSummaryProvider { "${(it as EditTextPreference).text}mB" }
            setOnPreferenceChangeListener { _, newValue ->
                if (newValue is String) {
                    if (newValue.isEmpty()) {
                        return@setOnPreferenceChangeListener false
                    }
                    val mB = newValue.toInt()
                    if (mB > 3000) {
                        return@setOnPreferenceChangeListener false
                    }
                    if (mB > 1000) {
                        Snackbar.make(
                            requireView(),
                            "Too much loudness will hurt your ears!!!",
                            Snackbar.LENGTH_LONG
                        ).show()
                    }
                    return@setOnPreferenceChangeListener true
                }
                return@setOnPreferenceChangeListener false
            }
        }

        updateRequestIgnoreBatteryOptimizations()

        findPreference<Preference>("version")!!.apply {
            summary =
                "${BuildConfig.VERSION_NAME}(${BuildConfig.VERSION_CODE})-${BuildConfig.BUILD_TYPE}"
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
                    } catch (e: IllegalArgumentException) {
                        Snackbar.make(
                            requireView(),
                            "Color String is not valid",
                            Snackbar.LENGTH_LONG
                        ).show()
                        return@setOnPreferenceChangeListener false
                    }
                    return@setOnPreferenceChangeListener true
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