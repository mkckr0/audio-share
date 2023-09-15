package io.github.mkckr0.audio_share_app.model

import android.app.Application
import android.content.Context
import android.media.AudioManager
import android.util.Log
import androidx.core.content.edit
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import androidx.preference.PreferenceManager
import io.github.mkckr0.audio_share_app.NetClient
import io.github.mkckr0.audio_share_app.R

class HomeViewModel(private val application: Application) : AndroidViewModel(application) {

    private val sharedPreferences by lazy { PreferenceManager.getDefaultSharedPreferences(application) }
    private val audioManager by lazy { application.getSystemService(Context.AUDIO_SERVICE) as AudioManager }
    private val netClient by lazy { NetClient(NetClientHandler()) }

    val volumeFrom = audioManager.getStreamMinVolume(AudioManager.STREAM_MUSIC).toFloat()
    val volumeTo = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC).toFloat()

    val workVolume = MutableLiveData<Int>().apply {
        value = sharedPreferences.getInt(
            "work_volume",
            audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC) / 2
        )
    }

    val idleVolume = MutableLiveData<Int>().apply {
        value = sharedPreferences.getInt(
            "idle_volume",
            audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        )
    }

    val host = MutableLiveData<String>().apply {
        value = sharedPreferences.getString("host", application.getString(R.string.default_host))
    }
    val port = MutableLiveData<String>().apply {
        value = sharedPreferences.getString("port", application.getString(R.string.default_port))
    }

    val hostError = MutableLiveData("")
    val portError = MutableLiveData("")

    val isPlaying = MutableLiveData(false)

    val info = MutableLiveData("")

    fun saveVolume() {
        sharedPreferences.edit(true) {
            putInt("work_volume", workVolume.value!!)
            putInt("idle_volume", idleVolume.value!!)
        }
    }

    fun onWorkVolumeChange(value: Int) {
        workVolume.value = value
        if (isPlaying.value!!) {
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, value, 0)
        }
    }

    fun onIdleVolumeChange(value: Int) {
        idleVolume.value = value
        if (!isPlaying.value!!) {
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, value, 0)
        }
    }

    fun startPlay() {
        if (netClient.isPlaying()) {
            netClient.stop()
        } else {
            if (host.value.isNullOrBlank()  || port.value.isNullOrBlank()) {
                if (host.value.isNullOrBlank()) {
                    hostError.value = "Host is Empty"
                }
                if (port.value.isNullOrBlank()) {
                    portError.value = "Port is Empty"
                }
                return
            }

            // save host and port
            sharedPreferences.edit()
                .putString("host", host.value)
                .putString("port", port.value)
                .apply()

            isPlaying.value = true
            info.value = application.getString(R.string.audio_starting)

            Log.d("AudioShare", "click start")

            netClient.start(host.value!!, port.value!!.toInt())
        }
    }

    inner class NetClientHandler : NetClient.Handler {
        override fun onNetError(e: String) {
            isPlaying.value = false
            info.value = e
        }

        override fun onAudioStop() {
            isPlaying.value = false
            info.value = application.getString(R.string.audio_stopped)
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, idleVolume.value!!, 0)
        }

        override fun onAudioStart() {
            info.value = application.getString(R.string.audio_started)
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, workVolume.value!!, 0)
        }
    }
}