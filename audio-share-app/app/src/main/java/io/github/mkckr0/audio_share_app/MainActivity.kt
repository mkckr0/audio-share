package io.github.mkckr0.audio_share_app

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.media.AudioManager
import android.net.Uri
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import io.github.mkckr0.audio_share_app.databinding.ActivityMainBinding


class MainActivity : AppCompatActivity(), NetClient.Handler {

    private lateinit var binding : ActivityMainBinding

    private val sharedPref by lazy { getPreferences(MODE_PRIVATE) }

    private val audioManager by lazy { getSystemService(Context.AUDIO_SERVICE) as AudioManager }
    private val powerManager by lazy { getSystemService(Context.POWER_SERVICE) as PowerManager }

    private val netClient = NetClient(this)

    private val packageUri by lazy { Uri.parse("package:${packageName}") }

    @SuppressLint("BatteryLife")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.buttonBatterySaverSettings.setOnClickListener {
            val intent = Intent(
                Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                packageUri
            )
            startActivity(intent)
        }

        binding.seekBarWorkVolume.max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        binding.seekBarWorkVolume.min = audioManager.getStreamMinVolume(AudioManager.STREAM_MUSIC)
        binding.seekBarWorkVolume.progress = sharedPref.getInt("work_volume", binding.seekBarWorkVolume.max / 2)
        binding.seekBarWorkVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (netClient.isPlaying()) {
                    audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, progress, 0)
                }
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
                if (seekBar != null) {
                    sharedPref.edit().putInt("work_volume", seekBar.progress).apply()
                }
            }
        })

        binding.seekBarIdleVolume.max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        binding.seekBarIdleVolume.min = audioManager.getStreamMinVolume(AudioManager.STREAM_MUSIC)
        binding.seekBarIdleVolume.progress = sharedPref.getInt(
            "idle_volume",
            audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        )
        binding.seekBarIdleVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!netClient.isPlaying()) {
                    audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, progress, 0)
                }
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
                if (seekBar != null) {
                    sharedPref.edit().putInt("idle_volume", seekBar.progress).apply()
                }
            }
        })

        binding.buttonStart.setOnClickListener { onButtonStartClick() }
        binding.buttonStop.setOnClickListener { onButtonStopClick() }

        binding.textViewInfo.movementMethod = ScrollingMovementMethod()

        binding.editTextHost.setText(sharedPref.getString("host", "192.168.3.2"))
        binding.editTextPort.setText(sharedPref.getString("port", "65530"))
    }

    private fun onButtonStartClick() {

        if (binding.editTextHost.text.isEmpty() || binding.editTextPort.text.isEmpty()) {
            Toast.makeText(applicationContext, "Please enter host and port", Toast.LENGTH_LONG)
                .show()
            return
        }

        // save host and port
        sharedPref.edit()
            .putString("host", binding.editTextHost.text.toString())
            .putString("port", binding.editTextPort.text.toString())
            .apply()

        binding.textViewInfo.text = getString(R.string.audio_starting)
        switchEnableState(false)

        Log.d("AudioShare", "click start")

        netClient.start(binding.editTextHost.text.toString(), binding.editTextPort.text.toString().toInt())
    }

    private fun onButtonStopClick() {
        netClient.stop()
    }

    private fun switchEnableState(b: Boolean) {
        binding.editTextHost.isEnabled = b
        binding.editTextPort.isEnabled = b
        binding.buttonStart.isEnabled = b
        binding.buttonStop.isEnabled = !b
    }

    override fun onNetError(e: String) {
        binding.textViewInfo.text = e
        switchEnableState(true)
    }

    override fun onAudioStop() {
        switchEnableState(true)
        binding.textViewInfo.text = getString(R.string.audio_stopped)
        audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, binding.seekBarIdleVolume.progress, 0)
    }

    override fun onAudioStart() {
        binding.textViewInfo.text = getString(R.string.audio_started)
        audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, binding.seekBarWorkVolume.progress, 0)
    }
}
