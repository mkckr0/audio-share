/*
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

package io.github.mkckr0.audio_share_app.service

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.audiofx.LoudnessEnhancer
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import androidx.annotation.OptIn
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.media3.common.MediaItem
import androidx.media3.common.MediaMetadata
import androidx.media3.common.Player
import androidx.media3.common.Player.Commands
import androidx.media3.common.SimpleBasePlayer
import androidx.media3.common.util.UnstableApi
import com.google.common.util.concurrent.Futures.immediateVoidFuture
import com.google.common.util.concurrent.ListenableFuture
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.AudioConfigKeys
import io.github.mkckr0.audio_share_app.model.NetworkConfigKeys
import io.github.mkckr0.audio_share_app.model.audioConfigDataStore
import io.github.mkckr0.audio_share_app.model.getFloat
import io.github.mkckr0.audio_share_app.model.getInteger
import io.github.mkckr0.audio_share_app.model.getResourceUri
import io.github.mkckr0.audio_share_app.model.networkConfigDataStore
import io.github.mkckr0.audio_share_app.pb.Client
import kotlinx.coroutines.CoroutineName
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.guava.future
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import java.nio.ByteBuffer
import kotlin.time.Duration.Companion.seconds

@OptIn(UnstableApi::class)
class AudioPlayer(val context: Context) : SimpleBasePlayer(Looper.getMainLooper()) {

    private val tag = AudioPlayer::class.simpleName

    private var _initState: State = State.Builder()
        .setAvailableCommands(
            Commands.Builder()
                .addAll(
                    COMMAND_PLAY_PAUSE,
                    COMMAND_STOP,
                    COMMAND_GET_CURRENT_MEDIA_ITEM,
                    COMMAND_GET_METADATA,
                    COMMAND_RELEASE,
                )
                .build()
        )
        .build()
    private var _state: State = _initState
    override fun getState(): State = _state

    private val netClient = NetClient(context.applicationContext)

    private var _audioTrack: AudioTrack? = null
    private val audioTrack get() = _audioTrack!!

    @Volatile
    private var _pipeline: AudioPipeline? = null
    private var _wifiLock: WifiManager.WifiLock? = null

    private var _loudnessEnhancer: LoudnessEnhancer? = null
    private val loudnessEnhancer get() = _loudnessEnhancer!!

    private val scope: CoroutineScope = MainScope()
    private val retryScope: CoroutineScope = MainScope()

    companion object {
        var message by mutableStateOf("")

        private const val MAX_QUEUED_MS = 200
        private const val OPUS_PACKET_MS = 20
        private const val PCM_DATAGRAM_BYTES = 1400

        // The server stops streaming after silence (2 s by default), so this must stay well above
        // normal packet jitter but low enough to let the audio output idle quickly.
        private const val IDLE_TIMEOUT_MS = 1500L
    }

    override fun handleSetPlayWhenReady(playWhenReady: Boolean): ListenableFuture<*> {
        return future {
            Log.d(tag, "handleSetPlayWhenReady playWhenReady=$playWhenReady")
            _state = state.buildUpon().setPlayerError(null).build()
            if (playWhenReady) {
                val networkConfig = context.networkConfigDataStore.data.first()
                val host = networkConfig[stringPreferencesKey(NetworkConfigKeys.HOST)]
                    ?: context.getString(R.string.default_host)
                val port = networkConfig[intPreferencesKey(NetworkConfigKeys.PORT)]
                    ?: context.getInteger(R.integer.default_port)

                val mediaItem = MediaItem.fromUri("tcp://$host:$port").buildUpon()
                    .setMediaMetadata(
                        MediaMetadata.Builder()
                            .setTitle("Audio Share")
                            .setArtist("$host:$port")
                            .setArtworkUri(context.getResourceUri(R.drawable.artwork))
                            .build()
                    )
                    .build()

                _state = state.buildUpon()
                    .setPlaylist(
                        listOf(
                            MediaItemData.Builder("media-1")
                                .setMediaItem(mediaItem)
                                .build()
                        )
                    )
                    .setCurrentMediaItemIndex(0)
                    .setPlaybackState(Player.STATE_BUFFERING)
                    .setPlayWhenReady(true, PLAY_WHEN_READY_CHANGE_REASON_USER_REQUEST)
                    .build()

                acquireWifiLock()
                netClient.start(
                    host = host,
                    port = port,
                    callback = NetClientCallBack()
                )
            } else {
                _state = state.buildUpon()
                    .setPlayWhenReady(false, PLAY_WHEN_READY_CHANGE_REASON_USER_REQUEST)
                    .build()
                netClient.stop()
                releaseAudio()
                releaseWifiLock()
                retryScope.coroutineContext.cancelChildren()
                message = context.getString(R.string.label_paused)
            }
        }
    }

    override fun handleStop(): ListenableFuture<*> {
        Log.d(tag, "handleStop")
        _state = _initState.buildUpon()
            .setPlaybackState(STATE_IDLE)
            .setPlayWhenReady(false, PLAY_WHEN_READY_CHANGE_REASON_USER_REQUEST)
            .build()
        netClient.stop()
        releaseAudio()
        releaseWifiLock()
        retryScope.coroutineContext.cancelChildren()
        message = context.getString(R.string.label_stopped)
        return immediateVoidFuture()
    }

    override fun handleRelease(): ListenableFuture<*> {
        Log.d(tag, "handleRelease")
        scope.cancel()
        netClient.stop()
        releaseAudio()
        releaseWifiLock()
        retryScope.cancel()
        _state = State.Builder().build()
        return immediateVoidFuture()
    }

    inner class NetClientCallBack : NetClient.Callback {
        private val tag = NetClientCallBack::class.simpleName

        override val scope: CoroutineScope = MainScope() + CoroutineName("NetClientCallbackScope")

        // The server stops sending while nothing is playing. Pause the AudioTrack in that
        // case so the audio hardware can idle too, and restart it when data comes back.
        private var lastDataTime = SystemClock.elapsedRealtime()
        private var audioIdle = false
        private var idleWatchdog: Job? = null

        override suspend fun log(message: String) {
//            Log.d(tag, "logMessage: $message")
            AudioPlayer.message = message
        }

        override suspend fun onReceiveAudioFormat(format: Client.AudioFormat) {
            val encoding = when (format.encoding) {
                Client.AudioFormat.Encoding.ENCODING_PCM_FLOAT -> AudioFormat.ENCODING_PCM_FLOAT
                Client.AudioFormat.Encoding.ENCODING_PCM_8BIT -> AudioFormat.ENCODING_PCM_8BIT
                Client.AudioFormat.Encoding.ENCODING_PCM_16BIT -> AudioFormat.ENCODING_PCM_16BIT
                Client.AudioFormat.Encoding.ENCODING_PCM_24BIT -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    AudioFormat.ENCODING_PCM_24BIT_PACKED
                } else {
                    AudioFormat.ENCODING_INVALID
                }

                Client.AudioFormat.Encoding.ENCODING_PCM_32BIT -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    AudioFormat.ENCODING_PCM_32BIT
                } else {
                    AudioFormat.ENCODING_INVALID
                }

                else -> {
                    AudioFormat.ENCODING_INVALID
                }
            }

            val channelMask = when (format.channels) {
                1 -> AudioFormat.CHANNEL_OUT_MONO
                2 -> AudioFormat.CHANNEL_OUT_STEREO
                3 -> AudioFormat.CHANNEL_OUT_STEREO or AudioFormat.CHANNEL_OUT_FRONT_CENTER
                4 -> AudioFormat.CHANNEL_OUT_QUAD
                5 -> AudioFormat.CHANNEL_OUT_QUAD or AudioFormat.CHANNEL_OUT_FRONT_CENTER
                6 -> AudioFormat.CHANNEL_OUT_5POINT1
                7 -> AudioFormat.CHANNEL_OUT_5POINT1 or AudioFormat.CHANNEL_OUT_BACK_CENTER
                8 -> AudioFormat.CHANNEL_OUT_7POINT1_SURROUND
                else -> AudioFormat.CHANNEL_INVALID
            }

            Log.i(
                tag,
                "encoding: $encoding, channelMask: $channelMask, sampleRate: ${format.sampleRate}, compression: ${format.compression}"
            )

            releaseAudio()
            var opusDecoder: OpusDecoder? = null
            try {
                when (format.compression) {
                    Client.AudioFormat.Compression.COMPRESSION_NONE -> {}
                    Client.AudioFormat.Compression.COMPRESSION_OPUS -> {
                        // MediaCodec decodes to 16-bit PCM, which is what the server declares.
                        opusDecoder = OpusDecoder(format.sampleRate, format.channels, format.opusPreSkip)
                    }

                    else -> throw Exception("Unsupported compression ${format.compression}, please update the app")
                }
            } catch (e: Exception) {
                Log.e(tag, e.stackTraceToString())
                onError(e.message, e)
                return
            }

            val minBufferSize =
                AudioTrack.getMinBufferSize(format.sampleRate, channelMask, encoding)

            Log.i(tag, "min buffer size: $minBufferSize bytes")

            val audioConfig = context.audioConfigDataStore.data.first()

            val bufferScale =
                (audioConfig[floatPreferencesKey(AudioConfigKeys.BUFFER_SCALE)] ?: context.getFloat(
                    R.string.default_buffer_scale
                )).toInt()
            Log.i(tag, "buffer scale: $bufferScale")

            val trackBuilder = AudioTrack.Builder()
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build()
                )
                .setAudioFormat(
                    AudioFormat.Builder()
                        .setEncoding(encoding)
                        .setChannelMask(channelMask)
                        .setSampleRate(format.sampleRate)
                        .build()
                )
                .setBufferSizeInBytes(minBufferSize * bufferScale)
                .setTransferMode(AudioTrack.MODE_STREAM)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                trackBuilder.setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
            }
            _audioTrack = trackBuilder.build()

            val volume = audioConfig[floatPreferencesKey(AudioConfigKeys.VOLUME)]
                ?: context.getFloat(R.string.default_volume)
            Log.i(tag, "volume: $volume")
            audioTrack.setVolume(volume)

            val loudnessEnhancerGain =
                (audioConfig[floatPreferencesKey(AudioConfigKeys.LOUDNESS_ENHANCER)]
                    ?: context.getFloat(R.string.default_loudness_enhancer)).toInt()
            Log.i(tag, "loudness enhancer: ${loudnessEnhancerGain}mB")
            if (loudnessEnhancerGain > 0) {
                _loudnessEnhancer = LoudnessEnhancer(audioTrack.audioSessionId)
                loudnessEnhancer.setTargetGain(loudnessEnhancerGain)
                loudnessEnhancer.setEnabled(true)
            }

            val bytesPerFrame = bytesPerSample(encoding) * format.channels
            val maxQueued = if (opusDecoder != null) {
                MAX_QUEUED_MS / OPUS_PACKET_MS
            } else {
                maxOf(MAX_QUEUED_MS / OPUS_PACKET_MS, format.sampleRate * bytesPerFrame * MAX_QUEUED_MS / 1000 / PCM_DATAGRAM_BYTES)
            }
            _pipeline = AudioPipeline(audioTrack, opusDecoder, bytesPerFrame, maxQueued) { stats ->
                message = "${context.getString(R.string.label_started)} · $stats"
            }.also { it.start() }
            lastDataTime = SystemClock.elapsedRealtime()
            audioIdle = false
        }

        override suspend fun onPlaybackStarted() {
            _state = state.buildUpon()
                .setPlaybackState(STATE_READY)
                .build()
            invalidateState()
            Log.d(tag, "onPlaybackStarted")
            message = context.getString(R.string.label_started)
            startIdleWatchdog()
        }

        private fun startIdleWatchdog() {
            idleWatchdog?.cancel()
            lastDataTime = SystemClock.elapsedRealtime()
            idleWatchdog = scope.launch {
                while (true) {
                    delay(1.seconds)
                    if (!audioIdle && SystemClock.elapsedRealtime() - lastDataTime > IDLE_TIMEOUT_MS) {
                        audioIdle = true
                        Log.d(tag, "no audio data, pausing AudioTrack")
                        _audioTrack?.run {
                            pause()
                            flush()
                        }
                    }
                }
            }
        }

        override fun onReceiveAudioData(audioData: ByteBuffer) {
//            Log.d(tag, "${audioData.remaining()}")
            lastDataTime = SystemClock.elapsedRealtime()
            if (audioIdle) {
                audioIdle = false
                Log.d(tag, "audio data again, resuming AudioTrack")
                _audioTrack?.play()
            }
            _pipeline?.submit(audioData)
        }

        override suspend fun onError(message: String?, cause: Throwable?) {
            // switch to retryScope to prevent NetClient cancel callback scope
            retryScope.launch {

                netClient.stop()

                val reason = message ?: cause?.stackTraceToString()
                var wait = 3
                while (wait > 0) {
                    log("$reason, ${context.getString(R.string.label_retry).format(wait)}")
                    delay(1.seconds)
                    --wait
                }

                _state = state.buildUpon()
                    .setPlayerError(null)
                    .setPlaybackState(Player.STATE_BUFFERING)
                    .build()
                invalidateState()

                val networkConfig = context.networkConfigDataStore.data.first()
                val host = networkConfig[stringPreferencesKey(NetworkConfigKeys.HOST)]
                    ?: context.getString(R.string.default_host)
                val port = networkConfig[intPreferencesKey(NetworkConfigKeys.PORT)]
                    ?: context.getInteger(R.integer.default_port)
                netClient.start(
                    host = host,
                    port = port,
                    callback = NetClientCallBack()
                )
            }
        }
    }

    // the pipeline owns the Opus decoder and has to stop before the track goes away
    private fun releaseAudio() {
        _pipeline?.shutdown()
        _pipeline = null
        _loudnessEnhancer?.run {
            release()
        }
        _loudnessEnhancer = null
        _audioTrack?.run {
            try {
                pause()
                flush()
            } catch (e: IllegalStateException) {
                Log.w(tag, e.stackTraceToString())
            }
            release()
        }
        _audioTrack = null
    }

    private fun bytesPerSample(encoding: Int): Int = when (encoding) {
        AudioFormat.ENCODING_PCM_8BIT -> 1
        AudioFormat.ENCODING_PCM_16BIT -> 2
        AudioFormat.ENCODING_PCM_24BIT_PACKED -> 3
        else -> 4
    }

    // Wi-Fi power save adds latency spikes to the UDP stream
    private fun acquireWifiLock() {
        if (_wifiLock?.isHeld == true) {
            return
        }
        try {
            val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
                ?: return
            val mode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                WifiManager.WIFI_MODE_FULL_LOW_LATENCY
            } else {
                @Suppress("DEPRECATION")
                WifiManager.WIFI_MODE_FULL_HIGH_PERF
            }
            _wifiLock = wifiManager.createWifiLock(mode, "AudioShare").apply {
                setReferenceCounted(false)
                acquire()
            }
        } catch (e: Exception) {
            Log.w(tag, e.stackTraceToString())
        }
    }

    private fun releaseWifiLock() {
        try {
            _wifiLock?.takeIf { it.isHeld }?.release()
        } catch (e: Exception) {
            Log.w(tag, e.stackTraceToString())
        }
        _wifiLock = null
    }

    /**
     * All exceptions in ListenableFuture will be suppressed, need log it
     */
    private fun future(block: suspend CoroutineScope.() -> Unit): ListenableFuture<*> {
        return scope.future {
            try {
                block()
            } catch (e: Exception) {
                Log.e(tag, e.stackTraceToString())
            }
        }
    }
}