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

import android.media.MediaCodec
import android.media.MediaFormat
import android.os.Build
import android.util.Log
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Decodes a stream of Opus packets (one per UDP datagram) to 16-bit PCM using the
 * platform's MediaCodec, so no native library is needed.
 *
 * Not thread safe: call everything from one thread.
 */
class OpusDecoder(sampleRate: Int, channels: Int, preSkip: Int = 0) {

    private val tag = OpusDecoder::class.simpleName

    private val codec: MediaCodec = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_AUDIO_OPUS)
    private val bufferInfo = MediaCodec.BufferInfo()
    private var ptsUs = 0L
    private var released = false

    init {
        val format = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_OPUS, sampleRate, channels)
        // The Opus decoder needs the three codec-specific buffers, see
        // https://developer.android.com/reference/android/media/MediaCodec#CSD
        format.setByteBuffer("csd-0", opusHead(sampleRate, channels, preSkip))
        format.setByteBuffer("csd-1", nanos(preSkip * 1_000_000_000L / 48_000))
        format.setByteBuffer("csd-2", nanos(SEEK_PRE_ROLL_NS))
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
        }
        try {
            codec.configure(format, null, null, 0)
            codec.start()
        } catch (e: Exception) {
            codec.release()
            throw e
        }
    }

    /**
     * Decode one Opus packet. `onPcm` is called for every chunk of 16-bit PCM the codec
     * has ready, which may lag the input by a frame or so.
     */
    fun decode(packet: ByteBuffer, onPcm: (ByteBuffer, Int) -> Unit) {
        if (released) {
            return
        }

        drain(onPcm)

        val inputIndex = codec.dequeueInputBuffer(INPUT_TIMEOUT_US)
        if (inputIndex < 0) {
            // Decoder is behind. Dropping a packet is better than blocking the network loop.
            Log.w(tag, "no input buffer available, dropping packet")
            return
        }
        val input = codec.getInputBuffer(inputIndex) ?: return
        input.clear()
        input.put(packet)
        codec.queueInputBuffer(inputIndex, 0, input.position(), ptsUs, 0)
        ptsUs += FRAME_US

        drain(onPcm)
    }

    private fun drain(onPcm: (ByteBuffer, Int) -> Unit) {
        while (true) {
            val outputIndex = codec.dequeueOutputBuffer(bufferInfo, 0)
            when {
                outputIndex >= 0 -> {
                    val output = codec.getOutputBuffer(outputIndex)
                    if (output != null && bufferInfo.size > 0) {
                        output.position(bufferInfo.offset)
                        output.limit(bufferInfo.offset + bufferInfo.size)
                        onPcm(output, bufferInfo.size)
                    }
                    codec.releaseOutputBuffer(outputIndex, false)
                }

                outputIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> continue
                else -> return // INFO_TRY_AGAIN_LATER
            }
        }
    }

    fun release() {
        if (released) {
            return
        }
        released = true
        try {
            codec.stop()
        } catch (e: Exception) {
            Log.w(tag, e.stackTraceToString())
        }
        codec.release()
    }

    companion object {
        private const val INPUT_TIMEOUT_US = 20_000L
        private const val FRAME_US = 20_000L          // the server always sends 20 ms packets
        private const val SEEK_PRE_ROLL_NS = 80_000_000L // 80 ms, the value used by Ogg Opus / Matroska

        /** "OpusHead" identification header, RFC 7845 section 5.1. */
        private fun opusHead(inputSampleRate: Int, channels: Int, preSkip: Int): ByteBuffer {
            return ByteBuffer.allocate(19).order(ByteOrder.LITTLE_ENDIAN).apply {
                put("OpusHead".toByteArray(Charsets.US_ASCII))
                put(1)                          // version
                put(channels.toByte())
                putShort(preSkip.toShort())
                putInt(inputSampleRate)
                putShort(0)                     // output gain
                put(0)                          // channel mapping family 0: mono / stereo
                flip()
            }
        }

        private fun nanos(value: Long): ByteBuffer {
            return ByteBuffer.allocate(Long.SIZE_BYTES).order(ByteOrder.nativeOrder()).apply {
                putLong(value)
                flip()
            }
        }
    }
}
