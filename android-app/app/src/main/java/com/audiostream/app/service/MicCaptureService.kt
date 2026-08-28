package com.audiostream.app.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.audiofx.AcousticEchoCanceler
import android.media.audiofx.NoiseSuppressor
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import com.audiostream.app.R
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder

class MicCaptureService : Service() {

    private val tag = "MicCaptureService"
    private var serviceJob: Job? = null
    private val scope = CoroutineScope(Dispatchers.IO)
    private var isRecording = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val action = intent?.action
        if (action == ACTION_STOP) {
            stopRecording()
            stopSelf()
            return START_NOT_STICKY
        }

        val host = intent?.getStringExtra(EXTRA_HOST) ?: "127.0.0.1"
        val port = intent?.getIntExtra(EXTRA_PORT, 65530) ?: 65530
        val sampleRate = intent?.getIntExtra(EXTRA_SAMPLE_RATE, 48000) ?: 48000
        val channels = intent?.getIntExtra(EXTRA_CHANNELS, 1) ?: 1

        startForegroundNotification()
        startRecording(host, port, sampleRate, channels)

        return START_STICKY
    }

    private fun startForegroundNotification() {
        val channelId = "audiostream_mic_channel"
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "Microphone Streaming",
                NotificationManager.IMPORTANCE_LOW
            )
            notificationManager.createNotificationChannel(channel)
        }

        val notification: Notification = NotificationCompat.Builder(this, channelId)
            .setContentTitle("AudioStream Microphone Active")
            .setContentText("Streaming microphone to PC virtual input...")
            .setSmallIcon(R.drawable.play_circle_white)
            .setOngoing(true)
            .build()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ServiceCompat.startForeground(
                this,
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun startRecording(host: String, port: Int, sampleRate: Int, channels: Int) {
        if (isRecording) return
        isRecording = true

        serviceJob = scope.launch {
            val channelConfig = if (channels == 2) AudioFormat.CHANNEL_IN_STEREO else AudioFormat.CHANNEL_IN_MONO
            val audioFormat = AudioFormat.ENCODING_PCM_16BIT
            val minBufSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)
            val bufferSize = maxOf(minBufSize, 4096)

            var audioRecord: AudioRecord? = null
            var socket: DatagramSocket? = null
            var aec: AcousticEchoCanceler? = null
            var ns: NoiseSuppressor? = null

            try {
                audioRecord = AudioRecord(
                    MediaRecorder.AudioSource.VOICE_COMMUNICATION,
                    sampleRate,
                    channelConfig,
                    audioFormat,
                    bufferSize
                )

                if (audioRecord.state != AudioRecord.STATE_INITIALIZED) {
                    Log.e(tag, "AudioRecord initialization failed")
                    return@launch
                }

                val sessionId = audioRecord.audioSessionId
                if (AcousticEchoCanceler.isAvailable()) {
                    aec = AcousticEchoCanceler.create(sessionId)?.apply { enabled = true }
                }
                if (NoiseSuppressor.isAvailable()) {
                    ns = NoiseSuppressor.create(sessionId)?.apply { enabled = true }
                }

                socket = DatagramSocket()
                val serverAddr = InetAddress.getByName(host)
                val readBuffer = ByteArray(960 * channels * 2) // 20ms frame buffer
                var sequence: Short = 0
                var timestamp = 0

                audioRecord.startRecording()
                Log.i(tag, "AudioRecord started streaming to $host:$port")

                while (isActive && isRecording) {
                    val bytesRead = audioRecord.read(readBuffer, 0, readBuffer.size)
                    if (bytesRead > 0) {
                        // Pack 7-byte binary header + audio payload
                        // Header layout: [PacketType (1B)][Sequence uint16 LE (2B)][Timestamp uint32 LE (4B)]
                        val packetBuffer = ByteBuffer.allocate(7 + bytesRead).order(ByteOrder.LITTLE_ENDIAN)
                        packetBuffer.put(0x02.toByte()) // Packet Type 0x02 = Phone Mic Audio
                        packetBuffer.putShort(sequence)
                        packetBuffer.putInt(timestamp)
                        packetBuffer.put(readBuffer, 0, bytesRead)

                        val packetData = packetBuffer.array()
                        val packet = DatagramPacket(packetData, packetData.size, serverAddr, port)
                        socket.send(packet)

                        sequence = (sequence + 1).toShort()
                        timestamp += (bytesRead / (channels * 2))
                    }
                }
            } catch (e: Exception) {
                Log.e(tag, "Error during mic capture/streaming: ${e.message}", e)
            } finally {
                try {
                    audioRecord?.stop()
                    audioRecord?.release()
                } catch (_: Exception) {}
                aec?.release()
                ns?.release()
                socket?.close()
                Log.i(tag, "AudioRecord streaming stopped")
            }
        }
    }

    private fun stopRecording() {
        isRecording = false
        serviceJob?.cancel()
        serviceJob = null
    }

    override fun onDestroy() {
        stopRecording()
        super.onDestroy()
    }

    companion object {
        const val NOTIFICATION_ID = 2001
        const val ACTION_START = "com.audiostream.app.START_MIC"
        const val ACTION_STOP = "com.audiostream.app.STOP_MIC"
        const val EXTRA_HOST = "extra_host"
        const val EXTRA_PORT = "extra_port"
        const val EXTRA_SAMPLE_RATE = "extra_sample_rate"
        const val EXTRA_CHANNELS = "extra_channels"

        fun start(context: Context, host: String, port: Int = 65530, sampleRate: Int = 48000, channels: Int = 1) {
            val intent = Intent(context, MicCaptureService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_HOST, host)
                putExtra(EXTRA_PORT, port)
                putExtra(EXTRA_SAMPLE_RATE, sampleRate)
                putExtra(EXTRA_CHANNELS, channels)
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            val intent = Intent(context, MicCaptureService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }
}
