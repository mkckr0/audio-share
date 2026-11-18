package io.github.mkckr0.audio_share_app

import io.github.mkckr0.audio_share_app.service.MicrophoneResources
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.IOException
import java.nio.channels.SocketChannel

class MicrophoneResourcesTest {
    @Test
    fun stoppingClosesAllResourcesAndIsIdempotent() {
        SocketChannel.open().use { first ->
            SocketChannel.open().use { second ->
                val session = MicrophoneResources()
                session.add(first)
                session.add(second)
                session.close()
                session.close()
                assertFalse(first.isOpen)
                assertFalse(second.isOpen)
            }
        }
    }

    @Test
    fun resourceCreatedAfterStopIsImmediatelyClosedAndRejected() {
        val oldSession = MicrophoneResources()
        oldSession.close()
        SocketChannel.open().use { channel ->
            assertThrows(IOException::class.java) { oldSession.add(channel) }
            assertFalse(channel.isOpen)
        }
    }
}
