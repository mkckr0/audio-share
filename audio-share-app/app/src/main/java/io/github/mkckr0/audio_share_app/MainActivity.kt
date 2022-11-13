package io.github.mkckr0.audio_share_app

import android.content.Context
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.room.Room
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch


class MainActivity : AppCompatActivity(), NetClient.Handler {

    private val editTextHost by lazy { findViewById<EditText>(R.id.editTextHost) }
    private val editTextPort by lazy { findViewById<EditText>(R.id.editTextPort) }

    private val buttonStart by lazy { findViewById<Button>(R.id.buttonStart) }
    private val buttonStop by lazy { findViewById<Button>(R.id.buttonStop) }

    private val textViewInfo by lazy { findViewById<TextView>(R.id.textViewInfo) }

    private lateinit var db: AppDatabase

    private val netClient = NetClient(this)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        buttonStart.setOnClickListener { onButtonStartClick() }
        buttonStop.setOnClickListener { onButtonStopClick() }

        textViewInfo.movementMethod = ScrollingMovementMethod()

        db = Room.databaseBuilder(applicationContext, AppDatabase::class.java, "db").build()

        MainScope().launch(Dispatchers.IO) {
            editTextHost.setText(db.settingsDao().getValue("host"))
            editTextPort.setText(db.settingsDao().getValue("port"))
            if (editTextHost.text.isEmpty()) {
                editTextHost.setText(R.string.host)
            }
            if (editTextPort.text.isEmpty()) {
                editTextPort.setText(R.string.port)
            }
        }
    }

    private fun onButtonStartClick() {

        if (editTextHost.text.isEmpty() || editTextPort.text.isEmpty()) {
            Toast.makeText(applicationContext, "Please enter host and port", Toast.LENGTH_LONG)
                .show()
            return
        }

        // save host and port
        MainScope().launch(Dispatchers.IO) {
            db.settingsDao().setValue(Settings("host", editTextHost.text.toString()))
            db.settingsDao().setValue(Settings("port", editTextPort.text.toString()))
        }

        textViewInfo.text = getString(R.string.audio_starting)
        switchEnableState(false)

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
    }

    override fun onAudioStart() {
        textViewInfo.text = getString(R.string.audio_started)
    }
}
