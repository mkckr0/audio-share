package io.github.mkckr0.audio_share_app

import io.github.mkckr0.audio_share_app.service.ServerDiscovery
import io.ktor.network.selector.SelectorManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.ServerSocket
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.concurrent.thread

class ServerDiscoveryTest {

    private fun host(a: Int, b: Int, c: Int, d: Int) = (a shl 24) or (b shl 16) or (c shl 8) or d

    @Test
    fun intToHost() {
        assertEquals("192.168.1.20", ServerDiscovery.intToHost(host(192, 168, 1, 20)))
    }

    @Test
    fun subnetHosts_24() {
        val hosts = ServerDiscovery.subnetHosts(host(192, 168, 1, 20), 24)
        assertEquals(254, hosts.size)
        assertEquals("192.168.1.1", hosts.first())
        assertEquals("192.168.1.254", hosts.last())
    }

    @Test
    fun subnetHosts_largeSubnetIsCapped() {
        val hosts = ServerDiscovery.subnetHosts(host(10, 0, 5, 7), 8)
        assertEquals(1022, hosts.size)
        assertEquals("10.0.4.1", hosts.first())
        assertEquals("10.0.7.254", hosts.last())
    }

    @Test
    fun probe_detectsServer() = runBlocking {
        ServerSocket(0).use { server ->
            thread {
                server.accept().use { peer ->
                    peer.getInputStream().readNBytes(4)
                    // reply CMD_GET_FORMAT
                    peer.getOutputStream().write(ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(1).array())
                    peer.getOutputStream().flush()
                    Thread.sleep(200)
                }
            }
            SelectorManager(Dispatchers.IO).use {
                assertTrue(ServerDiscovery.probe(it, "127.0.0.1", server.localPort))
            }
        }
    }

    @Test
    fun probe_ignoresClosedPort() = runBlocking {
        val port = ServerSocket(0).use { it.localPort }
        SelectorManager(Dispatchers.IO).use {
            assertFalse(ServerDiscovery.probe(it, "127.0.0.1", port))
        }
    }

    @Test
    fun probe_ignoresOtherService() = runBlocking {
        ServerSocket(0).use { server ->
            thread {
                server.accept().use { peer ->
                    peer.getOutputStream().write("HTTP/1.1 400 Bad Request\r\n\r\n".toByteArray())
                    peer.getOutputStream().flush()
                    Thread.sleep(200)
                }
            }
            SelectorManager(Dispatchers.IO).use {
                assertFalse(ServerDiscovery.probe(it, "127.0.0.1", server.localPort))
            }
        }
    }
}
