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
import android.widget.Button
import android.widget.EditText
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.ActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity


class MainActivity : AppCompatActivity(), NetClient.Handler {

    private val buttonBatterySaverSettings by lazy { findViewById<Button>(R.id.buttonBatterySaverSettings) }
    private val seekBarWorkVolume by lazy { findViewById<SeekBar>(R.id.seekBarWorkVolume) }
    private val seekBarIdleVolume by lazy { findViewById<SeekBar>(R.id.seekBarIdleVolume) }

    private val editTextHost by lazy { findViewById<EditText>(R.id.editTextHost) }
    private val editTextPort by lazy { findViewById<EditText>(R.id.editTextPort) }

    private val buttonStart by lazy { findViewById<Button>(R.id.buttonStart) }
    private val buttonStop by lazy { findViewById<Button>(R.id.buttonStop) }

    private val textViewInfo by lazy { findViewById<TextView>(R.id.textViewInfo) }

    private val sharedPref by lazy { getPreferences(MODE_PRIVATE) }

    private val audioManager by lazy { getSystemService(Context.AUDIO_SERVICE) as AudioManager }
    private val powerManager by lazy { getSystemService(Context.POWER_SERVICE) as PowerManager }

    private val netClient = NetClient(this)

    private val packageUri by lazy { Uri.parse("package:${packageName}") }

    @SuppressLint("BatteryLife")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        buttonBatterySaverSettings.setOnClickListener {
            val intent = Intent(
                Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                packageUri
            )
            startActivity(intent)
        }

        seekBarWorkVolume.max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        seekBarWorkVolume.min = audioManager.getStreamMinVolume(AudioManager.STREAM_MUSIC)
        seekBarWorkVolume.progress = sharedPref.getInt("work_volume", seekBarWorkVolume.max / 2)
        seekBarWorkVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
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

        seekBarIdleVolume.max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        seekBarIdleVolume.min = audioManager.getStreamMinVolume(AudioManager.STREAM_MUSIC)
        seekBarIdleVolume.progress = sharedPref.getInt(
            "idle_volume",
            audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        )
        seekBarIdleVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
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

        buttonStart.setOnClickListener { onButtonStartClick() }
        buttonStop.setOnClickListener { onButtonStopClick() }

        textViewInfo.movementMethod = ScrollingMovementMethod()

        editTextHost.setText(sharedPref.getString("host", "192.168.3.2"))
        editTextPort.setText(sharedPref.getString("port", "65530"))
    }

    private fun onButtonStartClick() {

        if (editTextHost.text.isEmpty() || editTextPort.text.isEmpty()) {
            Toast.makeText(applicationContext, "Please enter host and port", Toast.LENGTH_LONG)
                .show()
            return
        }

        // save host and port
        sharedPref.edit()
            .putString("host", editTextHost.text.toString())
            .putString("port", editTextPort.text.toString())
            .apply()

        textViewInfo.text = getString(R.string.audio_starting)
        switchEnableState(false)

        Log.d("AudioShare", "click start")

        netClient.start(editTextHost.text.toString(), editTextPort.text.toString().toInt())
    }

    private fun onButtonStopClick() {
        netClient.stop()
    }

    private fun switchEnableState(b: Boolean) {
        editTextHost.isEnabled = b
        editTextPort.isEnabled = b
        buttonStart.isEnabled = b
        buttonStop.isEnabled = !b
    }

    override fun onNetError(e: String) {
        textViewInfo.text = e
        switchEnableState(true)
    }

    override fun onAudioStop() {
        switchEnableState(true)
        textViewInfo.text = getString(R.string.audio_stopped)
        audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, seekBarIdleVolume.progress, 0)
    }

    override fun onAudioStart() {
        textViewInfo.text = getString(R.string.audio_started)
        audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, seekBarWorkVolume.progress, 0)
    }
}
