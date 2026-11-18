package io.github.mkckr0.audio_share_app.service

import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder

internal object MicrophoneProtocol {
    const val SAMPLE_RATE = 48000
    const val CHANNELS = 1
    const val BITS_PER_SAMPLE = 16
    const val SAMPLES_PER_FRAME = 960
    const val MAX_FRAME_BYTES = 1920

    fun header(): ByteArray = ByteBuffer.allocate(20).order(ByteOrder.LITTLE_ENDIAN)
        .put("ASMIC002".toByteArray(Charsets.US_ASCII))
        .putInt(SAMPLE_RATE).putInt(CHANNELS).putInt(BITS_PER_SAMPLE).array()

    fun frame(samples: ShortArray, sampleCount: Int, target: ByteBuffer): ByteBuffer {
        require(sampleCount in 1..SAMPLES_PER_FRAME && sampleCount <= samples.size)
        require(target.capacity() >= 4 + sampleCount * 2)
        target.clear()
        target.order(ByteOrder.LITTLE_ENDIAN)
        target.putInt(sampleCount * 2)
        for (index in 0 until sampleCount) target.putShort(samples[index])
        target.flip()
        return target
    }

    fun requireAcknowledgement(reply: ByteArray) {
        if (!reply.contentEquals(byteArrayOf(0x4f, 0x4b, 0x41, 0x59))) {
            throw IOException("Microphone receiver rejected the audio format")
        }
    }
}
