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
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.common.Player.Commands
import androidx.media3.common.SimpleBasePlayer
import androidx.media3.common.util.UnstableApi
import com.google.common.util.concurrent.Futures.immediateVoidFuture
import com.google.common.util.concurrent.ListenableFuture
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.getFloat
import io.github.mkckr0.audio_share_app.model.AudioConfigKeys
import io.github.mkckr0.audio_share_app.model.audioConfigDataStore
import io.github.mkckr0.audio_share_app.pb.Client
import kotlinx.coroutines.CoroutineName
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.plus
import java.nio.ByteBuffer
import kotlin.time.Duration.Companion.seconds

@OptIn(UnstableApi::class)
class AudioPlayer(val context: Context) : SimpleBasePlayer(Looper.getMainLooper()) {

    private val tag = AudioPlayer::class.simpleName
    private var _state: State
    override fun getState(): State = _state

    private var _netClient: NetClient? = null
    private val netClient get() = _netClient!!

    private var _audioTrack: AudioTrack? = null
    private val audioTrack get() = _audioTrack!!

    private var _loudnessEnhancer: LoudnessEnhancer? = null
    private val loudnessEnhancer get() = _loudnessEnhancer!!

    private val audioPlayerScope = MainScope() + CoroutineName("AudioPlayerScope")

    private lateinit var _mediaItem: MediaItem

    companion object {
        var message by mutableStateOf("")
    }

    init {
        _state = State.Builder()
            .setAvailableCommands(
                Commands.Builder()
                    .addAll(
                        COMMAND_PREPARE,
                        COMMAND_PLAY_PAUSE,
                        COMMAND_SET_MEDIA_ITEM,
                        COMMAND_GET_CURRENT_MEDIA_ITEM,
                        COMMAND_GET_METADATA,
                    )
                    .build()
            )
            .build()
    }

    override fun handleSetMediaItems(
        mediaItems: MutableList<MediaItem>,
        startIndex: Int,
        startPositionMs: Long
    ): ListenableFuture<*> {
        Log.d(tag, "handleSetMediaItems ${mediaItems.first().localConfiguration?.uri}")
        _mediaItem = mediaItems.first()
        return immediateVoidFuture()
    }

    override fun handlePrepare(): ListenableFuture<*> {
        Log.d(tag, "handlePrepare")
        _state = state.buildUpon()
            .setPlaylist(listOf(
                MediaItemData.Builder("media-1")
                    .setMediaItem(_mediaItem)
                    .build()
            ))
            .setCurrentMediaItemIndex(0)
            .build()
        invalidateState()   // update currentMediaItem
        currentMediaItem?.localConfiguration?.uri?.let {
            _netClient = NetClient().apply {
                configure(it.host!!, it.port, NetClientCallBack())
            }
        }
        return immediateVoidFuture()
    }

    override fun handleSetPlayWhenReady(playWhenReady: Boolean): ListenableFuture<*> {
        _state = state.buildUpon().setPlayerError(null).build()
        Log.d(tag, "handleSetPlayWhenReady playWhenReady=$playWhenReady")
        if (playWhenReady) {
            _state = state.buildUpon()
                .setPlaybackState(Player.STATE_BUFFERING)
                .setPlayWhenReady(true, PLAY_WHEN_READY_CHANGE_REASON_USER_REQUEST)
                .build()
            netClient.start()
        } else {
            _state = state.buildUpon()
                .setPlaybackState(STATE_ENDED)
                .setPlayWhenReady(false, PLAY_WHEN_READY_CHANGE_REASON_USER_REQUEST)
                .build()
            netClient.stop()
            audioPlayerScope.coroutineContext.cancelChildren()
            message = "stopped"
        }
        return immediateVoidFuture()
    }

    override fun handleRelease(): ListenableFuture<*> {
        _netClient?.stop()
        _netClient = null
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
        return immediateVoidFuture()
    }

    inner class NetClientCallBack : NetClient.Callback {
        private val tag = NetClientCallBack::class.simpleName

        override val scope: CoroutineScope = audioPlayerScope

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

            Log.i(tag, "encoding: $encoding, channelMask: $channelMask, sampleRate: ${format.sampleRate}")

            val minBufferSize = AudioTrack.getMinBufferSize(format.sampleRate, channelMask, encoding)

            Log.i(tag, "min buffer size: $minBufferSize bytes")

            val audioConfig = context.audioConfigDataStore.data.first()

            val bufferScale = (audioConfig[floatPreferencesKey(AudioConfigKeys.BUFFER_SCALE)] ?: context.getFloat(R.string.default_buffer_scale)).toInt()
            Log.i(tag, "buffer scale: $bufferScale")

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

            val volume = audioConfig[floatPreferencesKey(AudioConfigKeys.VOLUME)] ?: context.getFloat(R.string.default_volume)
            Log.i(tag, "volume: $volume")
            audioTrack.setVolume(volume)

            val loudnessEnhancerGain = (audioConfig[floatPreferencesKey(AudioConfigKeys.LOUDNESS_ENHANCER)] ?: context.getFloat(R.string.default_loudness_enhancer)).toInt()
            Log.i(tag, "loudness enhancer: ${loudnessEnhancerGain}mB")
            if (loudnessEnhancerGain > 0) {
                _loudnessEnhancer = LoudnessEnhancer(audioTrack.audioSessionId)
                loudnessEnhancer.setTargetGain(loudnessEnhancerGain)
                loudnessEnhancer.setEnabled(true)
            }

            audioTrack.play()
        }

        override suspend fun onPlaybackStarted() {
            _state = state.buildUpon()
                .setPlaybackState(STATE_READY)
                .build()
            invalidateState()
            Log.d(tag, "onPlaybackStarted")
            message = "started"
        }

        override suspend fun onReceiveAudioData(audioData: ByteBuffer) {
            audioTrack.write(audioData, audioData.remaining(), AudioTrack.WRITE_NON_BLOCKING)
        }

        override suspend fun onError(message: String?, cause: Throwable?) {

            netClient.stop()

            // retry
            delay(3.seconds)
            _state = state.buildUpon()
                .setPlayerError(null)
                .setPlaybackState(Player.STATE_BUFFERING)
                .build()
            invalidateState()
            netClient.start()
        }
    }

}