package io.github.mkckr0.audio_share_app

import io.github.mkckr0.audio_share_app.service.MicrophoneProtocol
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.IOException
import java.nio.ByteBuffer

class MicrophoneProtocolTest {
    @Test
    fun headerMatchesIndependentWireFixture() {
        assertArrayEquals(
            byteArrayOf(0x41, 0x53, 0x4d, 0x49, 0x43, 0x30, 0x30, 0x32,
                0x80.toByte(), 0xbb.toByte(), 0, 0, 1, 0, 0, 0, 16, 0, 0, 0),
            MicrophoneProtocol.header()
        )
    }

    @Test
    fun frameEncodesSignedSamplesInLittleEndianOrder() {
        val target = ByteBuffer.allocate(1924)
        val frame = MicrophoneProtocol.frame(shortArrayOf(-32768, -1, 0, 32767), 4, target)
        val actual = ByteArray(frame.remaining()).also { frame.get(it) }
        assertArrayEquals(
            byteArrayOf(8, 0, 0, 0, 0, 0x80.toByte(), 0xff.toByte(), 0xff.toByte(),
                0, 0, 0xff.toByte(), 0x7f), actual
        )
    }

    @Test
    fun fullTwentyMillisecondFrameUsesBoundedReusableBuffer() {
        val target = ByteBuffer.allocate(1924)
        val frame = MicrophoneProtocol.frame(ShortArray(960) { 1 }, 960, target)
        assertEquals(1924, frame.remaining())
        assertArrayEquals(byteArrayOf(0x80.toByte(), 7, 0, 0, 1, 0),
            ByteArray(6).also { frame.get(it) })
        val next = MicrophoneProtocol.frame(shortArrayOf(2), 1, target)
        assertArrayEquals(byteArrayOf(2, 0, 0, 0, 2, 0),
            ByteArray(next.remaining()).also { next.get(it) })
    }

    @Test
    fun rejectsEmptyOversizedAndOutOfBoundsSamples() {
        for (count in intArrayOf(0, -1, 961, 1000)) {
            assertThrows(IllegalArgumentException::class.java) {
                MicrophoneProtocol.frame(ShortArray(960), count, ByteBuffer.allocate(1924))
            }
        }
        assertThrows(IllegalArgumentException::class.java) {
            MicrophoneProtocol.frame(shortArrayOf(1), 2, ByteBuffer.allocate(1924))
        }
    }

    @Test
    fun requiresExactServerAcknowledgement() {
        MicrophoneProtocol.requireAcknowledgement(byteArrayOf(0x4f, 0x4b, 0x41, 0x59))
        for (reply in listOf(byteArrayOf(), byteArrayOf(0x4f, 0x4b, 0x41),
            byteArrayOf(0x4e, 0x4f, 0x50, 0x45), byteArrayOf(0x4f, 0x4b, 0x41, 0x59, 0))) {
            assertThrows(IOException::class.java) {
                MicrophoneProtocol.requireAcknowledgement(reply)
            }
        }
    }
}
