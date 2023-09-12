package io.github.mkckr0.audio_share_app.ui

import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.util.Log
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import io.github.mkckr0.audio_share_app.BuildConfig
import io.github.mkckr0.audio_share_app.R

class SettingsFragment : PreferenceFragmentCompat() {

    private val tag = javaClass.simpleName

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {

        setPreferencesFromResource(R.xml.root_preferences, rootKey)

        updateRequestIgnoreBatteryOptimizations()

        findPreference<Preference>("version")!!.apply {
            summary = "${BuildConfig.VERSION_NAME}(${BuildConfig.VERSION_CODE})-${BuildConfig.BUILD_TYPE}"
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

    private fun updateRequestIgnoreBatteryOptimizations() : Boolean {
        val powerManager = requireActivity().getSystemService(Context.POWER_SERVICE) as PowerManager
        val preference = findPreference<Preference>("request_ignore_battery_optimizations")!!
        val newSummary = if (powerManager.isIgnoringBatteryOptimizations(requireContext().packageName)) {
            "Ignored"
        } else {
            "Not Ignored"
        }
        val changed = newSummary != preference.summary
        preference.summary = newSummary
        return changed
    }
}