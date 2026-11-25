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
import android.os.Build
import android.os.Looper
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
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.guava.future
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import java.nio.ByteBuffer
import java.util.concurrent.Executors
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

    private var _loudnessEnhancer: LoudnessEnhancer? = null
    private val loudnessEnhancer get() = _loudnessEnhancer!!

    private val scope: CoroutineScope = MainScope()
    private val retryScope: CoroutineScope = MainScope()

    // ------------------------------------------------------------------
    // 音频写入路径（为什么不直接 write(WRITE_NON_BLOCKING) 见下面说明）
    // 上游实现每包一次 `audioTrack.write(buf, size, WRITE_NON_BLOCKING)`、
    // 跑在 Main 线程、返回值被丢弃 ⇒ AudioTrack 缓冲只有 1x minBufferSize
    // (15392 B ≈ 40 ms) 时写不进去的样本被静默丢弃，实测稳态 5% 欠载
    // （AudioFlinger Underruns 每秒约 +2500 帧）⇒ 每 21 ms 一次爆音。
    // 现在：UDP 读完只入队（非阻塞），由单线程按序 BLOCKING 写满，缓冲满时
    // 反压到队列而不是丢样本；队列溢出时丢最旧数据保实时。
    // ------------------------------------------------------------------
    private val audioQueue = Channel<ByteBuffer>(capacity = 128)
    private val audioWriterExecutor = Executors.newSingleThreadExecutor { r ->
        Thread(r, "AudioWriter").apply { isDaemon = true }
    }
    private val audioWriterScope = CoroutineScope(
        SupervisorJob() + audioWriterExecutor.asCoroutineDispatcher() + CoroutineName("AudioWriter")
    )
    private var _audioWriterJob: Job? = null

    private fun dropPendingAudio() {
        while (audioQueue.tryReceive().isSuccess) {
            // drain pending packets
        }
    }

    private fun startAudioWriter() {
        if (_audioWriterJob != null) return
        _audioWriterJob = audioWriterScope.launch {
            for (buf in audioQueue) {
                val track = _audioTrack ?: continue
                while (buf.remaining() > 0) {
                    val written = try {
                        track.write(buf, buf.remaining(), AudioTrack.WRITE_BLOCKING)
                    } catch (e: IllegalStateException) {
                        Log.w(tag, "write on released track: ${e.message}")
                        break
                    }
                    if (written <= 0) {
                        Log.w(tag, "AudioTrack.write returned $written, aborting chunk")
                        break
                    }
                }
            }
        }
    }

    companion object {
        var message by mutableStateOf("")
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
                dropPendingAudio()
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
        dropPendingAudio()
        retryScope.coroutineContext.cancelChildren()
        message = context.getString(R.string.label_stopped)
        return immediateVoidFuture()
    }

    override fun handleRelease(): ListenableFuture<*> {
        Log.d(tag, "handleRelease")
        scope.cancel()
        netClient.stop()
        dropPendingAudio()
        _audioWriterJob?.cancel()
        _audioWriterJob = null
        retryScope.cancel()
        _loudnessEnhancer?.run {
            release()
        }
        _loudnessEnhancer = null
        _audioTrack?.run {
            pause()
            flush()
            release()
        }
        _audioTrack = null
        _state = State.Builder().build()
        return immediateVoidFuture()
    }

    inner class NetClientCallBack : NetClient.Callback {
        private val tag = NetClientCallBack::class.simpleName

        override val scope: CoroutineScope = MainScope() + CoroutineName("NetClientCallbackScope")

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
                "encoding: $encoding, channelMask: $channelMask, sampleRate: ${format.sampleRate}"
            )

            val minBufferSize =
                AudioTrack.getMinBufferSize(format.sampleRate, channelMask, encoding)

            Log.i(tag, "min buffer size: $minBufferSize bytes")

            val audioConfig = context.audioConfigDataStore.data.first()

            val bufferScale =
                (audioConfig[floatPreferencesKey(AudioConfigKeys.BUFFER_SCALE)] ?: context.getFloat(
                    R.string.default_buffer_scale
                )).toInt()
            Log.i(tag, "buffer scale: $bufferScale")

            dropPendingAudio()
            startAudioWriter()

            _audioTrack = AudioTrack.Builder()
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
                .build()

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

            audioTrack.play()
            Log.i(tag, "AudioTrack started, buffer=${audioTrack.bufferSizeInFrames} frames")
        }

        override suspend fun onPlaybackStarted() {
            _state = state.buildUpon()
                .setPlaybackState(STATE_READY)
                .build()
            invalidateState()
            Log.d(tag, "onPlaybackStarted")
            message = context.getString(R.string.label_started)
        }

        override suspend fun onReceiveAudioData(audioData: ByteBuffer) {
            // 只入队；阻塞写由 AudioWriter 单线程完成，顺序由队列保证。
            while (true) {
                if (audioQueue.trySend(audioData).isSuccess) return
                if (audioQueue.isClosedForSend) return
                // 队列满：丢最旧的一包保实时，避免延迟累积
                audioQueue.tryReceive()
            }
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