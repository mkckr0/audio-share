package io.github.mkckr0.audio_share_app.service

import android.Manifest
import android.annotation.SuppressLint
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.audiofx.AcousticEchoCanceler
import android.media.audiofx.AudioEffect
import android.media.audiofx.NoiseSuppressor
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.annotation.StringRes
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.ui.MainActivity
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.Closeable
import java.io.IOException
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.util.concurrent.TimeUnit

enum class MicrophoneStatus(@StringRes val message: Int) {
    OFF(R.string.microphone_off),
    CONNECTING(R.string.microphone_connecting),
    STREAMING(R.string.microphone_streaming),
    RECONNECTING(R.string.microphone_reconnecting),
    UNAVAILABLE(R.string.microphone_unavailable),
    PERMISSION_REQUIRED(R.string.microphone_permission_required),
    START_FAILED(R.string.microphone_start_failed),
}

data class MicrophoneState(
    val enabled: Boolean = false,
    val status: MicrophoneStatus = MicrophoneStatus.OFF,
)

/** User-started microphone channel, independent of the speaker playback service. */
class MicrophoneService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    // Accessed only on the main dispatcher. A cancelled session cannot publish over its successor.
    private var activeSession: Session? = null

    private class Session {
        val resources = MicrophoneResources()
        var job: Job? = null
    }

    override fun onCreate() {
        super.onCreate()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            getSystemService(NotificationManager::class.java).createNotificationChannel(
                NotificationChannel(CHANNEL_ID, getString(R.string.microphone_notification_channel),
                    NotificationManager.IMPORTANCE_LOW)
            )
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> startCapture()
            ACTION_STOP -> stopCapture()
            else -> stopCapture()
        }
        // The microphone must never resume after process death without another user action.
        return START_NOT_STICKY
    }

    private fun startCapture() {
        if (activeSession != null) return
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) !=
            PackageManager.PERMISSION_GRANTED) {
            stopCapture(MicrophoneStatus.PERMISSION_REQUIRED)
            return
        }
        try {
            ServiceCompat.startForeground(this, NOTIFICATION_ID,
                notification(MicrophoneStatus.CONNECTING),
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE else 0)
        } catch (error: RuntimeException) {
            Log.w(TAG, "Cannot start microphone foreground service", error)
            stopCapture(MicrophoneStatus.START_FAILED)
            return
        }
        val session = Session()
        activeSession = session
        mutableState.value = MicrophoneState(true, MicrophoneStatus.CONNECTING)
        session.job = scope.launch(Dispatchers.IO) { capture(session) }
    }

    private suspend fun capture(session: Session) {
        val samples = ShortArray(MicrophoneProtocol.SAMPLES_PER_FRAME)
        val frame = ByteBuffer.allocate(4 + MicrophoneProtocol.MAX_FRAME_BYTES)
        while (currentCoroutineContext().isActive) {
            var connection: MicrophoneConnection? = null
            var recording: Recording? = null
            try {
                publish(session, MicrophoneStatus.CONNECTING)
                connection = session.resources.add(MicrophoneConnection(
                    InetSocketAddress("127.0.0.1", 65529), IO_TIMEOUT_MS))
                connection.connectAndHandshake()
                currentCoroutineContext().ensureActive()
                // No AudioRecord is opened until Windows has accepted its output endpoint.
                recording = session.resources.add(Recording.open())
                recording.start()
                publish(session, MicrophoneStatus.STREAMING)
                var count = 0
                var lastRead = System.nanoTime()
                while (currentCoroutineContext().isActive) {
                    val read = recording.read(samples, count, samples.size - count)
                    if (read == 0) {
                        if (System.nanoTime() - lastRead > TimeUnit.MILLISECONDS.toNanos(IO_TIMEOUT_MS)) {
                            throw RecordingException("Microphone stopped delivering samples")
                        }
                        delay(2)
                        continue
                    }
                    lastRead = System.nanoTime()
                    count += read
                    if (count == samples.size) {
                        connection.writeFrame(MicrophoneProtocol.frame(samples, count, frame))
                        count = 0
                    }
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: SecurityException) {
                withContext(Dispatchers.Main.immediate) {
                    if (activeSession === session) stopCapture(MicrophoneStatus.PERMISSION_REQUIRED)
                }
                return
            } catch (error: Exception) {
                currentCoroutineContext().ensureActive()
                Log.w(TAG, "Microphone session interrupted", error)
                publish(session, if (error is RecordingException)
                    MicrophoneStatus.UNAVAILABLE else MicrophoneStatus.RECONNECTING)
            } finally {
                recording?.let { session.resources.release(it) }
                connection?.let { session.resources.release(it) }
            }
            delay(RETRY_DELAY_MS)
        }
    }

    private suspend fun publish(session: Session, status: MicrophoneStatus) {
        withContext(Dispatchers.Main.immediate) {
            if (activeSession === session) {
                mutableState.value = MicrophoneState(true, status)
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU ||
                    ContextCompat.checkSelfPermission(this@MicrophoneService, Manifest.permission.POST_NOTIFICATIONS)
                    == PackageManager.PERMISSION_GRANTED) {
                    getSystemService(NotificationManager::class.java)
                        .notify(NOTIFICATION_ID, notification(status))
                }
            }
        }
    }

    private fun stopCapture(status: MicrophoneStatus = MicrophoneStatus.OFF) {
        val session = activeSession
        activeSession = null
        mutableState.value = MicrophoneState(false, status)
        session?.job?.cancel()
        // Closing on the caller immediately unblocks socket waits and releases the microphone.
        session?.resources?.close()
        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun notification(status: MicrophoneStatus): Notification {
        val open = PendingIntent.getActivity(this, 2, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        val stop = PendingIntent.getService(this, 1,
            Intent(this, MicrophoneService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.microphone)
            .setContentTitle(getString(R.string.microphone_notification_title))
            .setContentText(getString(status.message))
            .setContentIntent(open)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setSilent(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setForegroundServiceBehavior(NotificationCompat.FOREGROUND_SERVICE_IMMEDIATE)
            .addAction(0, getString(R.string.microphone_stop), stop)
            .build()
    }

    override fun onDestroy() {
        val session = activeSession
        activeSession = null
        session?.job?.cancel()
        session?.resources?.close()
        if (session != null) mutableState.value = MicrophoneState()
        scope.cancel()
        super.onDestroy()
    }

    companion object {
        private const val TAG = "MicrophoneService"
        private const val CHANNEL_ID = "microphone"
        private const val NOTIFICATION_ID = 65529
        private const val ACTION_START = "microphone.START"
        private const val ACTION_STOP = "microphone.STOP"
        private const val IO_TIMEOUT_MS = 3000L
        private const val RETRY_DELAY_MS = 1500L
        private val mutableState = MutableStateFlow(MicrophoneState())
        val state: StateFlow<MicrophoneState> = mutableState.asStateFlow()

        fun start(context: Context) {
            try {
                ContextCompat.startForegroundService(context,
                    Intent(context, MicrophoneService::class.java).setAction(ACTION_START))
            } catch (error: RuntimeException) {
                Log.w(TAG, "Microphone start was denied", error)
                mutableState.value = MicrophoneState(false, MicrophoneStatus.START_FAILED)
            }
        }

        fun stop(context: Context) {
            context.startService(Intent(context, MicrophoneService::class.java).setAction(ACTION_STOP))
        }
    }
}

private class RecordingException(message: String, cause: Throwable? = null) : IOException(message, cause)

@SuppressLint("MissingPermission")
private class Recording private constructor(private val record: AudioRecord) : Closeable {
    private val lock = Any()
    private var closed = false
    private val effects = mutableListOf<AudioEffect>()

    init {
        // Effects are best effort; the platform may already apply voice processing for this source.
        tryEffect { if (AcousticEchoCanceler.isAvailable()) AcousticEchoCanceler.create(record.audioSessionId) else null }
        tryEffect { if (NoiseSuppressor.isAvailable()) NoiseSuppressor.create(record.audioSessionId) else null }
    }

    private fun tryEffect(create: () -> AudioEffect?) {
        var effect: AudioEffect? = null
        try {
            effect = create()
            if (effect != null && effect.setEnabled(true) == AudioEffect.SUCCESS) {
                effects.add(effect)
            } else {
                effect?.release()
            }
        } catch (error: RuntimeException) {
            runCatching { effect?.release() }
            Log.d("MicrophoneService", "Voice processing effect unavailable", error)
        }
    }

    fun start() = synchronized(lock) {
        if (closed) throw RecordingException("Microphone session stopped")
        try {
            record.startRecording()
            if (record.recordingState != AudioRecord.RECORDSTATE_RECORDING) {
                throw RecordingException("Microphone could not start recording")
            }
        } catch (error: IllegalStateException) {
            throw RecordingException("Microphone could not start recording", error)
        }
    }

    fun read(samples: ShortArray, offset: Int, length: Int): Int = synchronized(lock) {
        if (closed) throw RecordingException("Microphone session stopped")
        val count = record.read(samples, offset, length, AudioRecord.READ_NON_BLOCKING)
        if (count < 0) throw RecordingException("Microphone read failed: $count")
        count
    }

    override fun close() = synchronized(lock) {
        if (closed) return
        closed = true
        try {
            if (record.recordingState == AudioRecord.RECORDSTATE_RECORDING) record.stop()
        } catch (_: IllegalStateException) {
            // The audio device can disappear during capture.
        } finally {
            effects.forEach { runCatching { it.release() } }
            effects.clear()
            record.release()
        }
    }

    companion object {
        fun open(): Recording {
            try {
                val minimum = AudioRecord.getMinBufferSize(MicrophoneProtocol.SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
                if (minimum <= 0) throw RecordingException("48 kHz microphone recording is unavailable")
                val audio = AudioRecord.Builder()
                    .setAudioSource(MediaRecorder.AudioSource.VOICE_COMMUNICATION)
                    .setAudioFormat(AudioFormat.Builder()
                        .setSampleRate(MicrophoneProtocol.SAMPLE_RATE)
                        .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT).build())
                    .setBufferSizeInBytes(maxOf(minimum * 2, MicrophoneProtocol.MAX_FRAME_BYTES * 4))
                    .build()
                if (audio.state != AudioRecord.STATE_INITIALIZED) {
                    audio.release()
                    throw RecordingException("Microphone initialization failed")
                }
                try {
                    return Recording(audio)
                } catch (error: Exception) {
                    audio.release()
                    throw error
                }
            } catch (error: SecurityException) {
                throw error
            } catch (error: RuntimeException) {
                throw RecordingException("Microphone initialization failed", error)
            }
        }
    }
}
