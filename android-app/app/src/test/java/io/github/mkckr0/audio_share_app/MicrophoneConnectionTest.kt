package io.github.mkckr0.audio_share_app

import io.github.mkckr0.audio_share_app.service.MicrophoneConnection
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.DataInputStream
import java.io.IOException
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.SocketTimeoutException
import java.nio.ByteBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException

class MicrophoneConnectionTest {
    private val header = byteArrayOf(0x41, 0x53, 0x4d, 0x49, 0x43, 0x30, 0x30, 0x32,
        0x80.toByte(), 0xbb.toByte(), 0, 0, 1, 0, 0, 0, 16, 0, 0, 0)

    @Test(timeout = 10000)
    fun acceptsFragmentedAckAndReconnectsWithFreshHandshake() {
        ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
            server.soTimeout = 2000
            val executor = Executors.newSingleThreadExecutor()
            try {
                val received = executor.submit<List<ByteArray>> {
                    List(2) {
                        server.accept().use { socket ->
                            socket.soTimeout = 2000
                            val input = DataInputStream(socket.getInputStream())
                            val actualHeader = ByteArray(20).also { input.readFully(it) }
                            assertArrayEquals(header, actualHeader)
                            socket.getOutputStream().write(byteArrayOf(0x4f, 0x4b))
                            socket.getOutputStream().flush()
                            socket.getOutputStream().write(byteArrayOf(0x41, 0x59))
                            ByteArray(6).also {
                                input.readFully(it)
                                socket.getOutputStream().write(0x06)
                            }
                        }
                    }
                }
                repeat(2) {
                    MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 2000).use {
                        it.connectAndHandshake()
                        it.writeFrame(ByteBuffer.wrap(byteArrayOf(2, 0, 0, 0, 1, 0)))
                    }
                }
                received.get(5, TimeUnit.SECONDS).forEach {
                    assertArrayEquals(byteArrayOf(2, 0, 0, 0, 1, 0), it)
                }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @Test(timeout = 10000)
    fun rejectsTruncatedAndIncorrectAcknowledgements() {
        for (reply in listOf(byteArrayOf(), byteArrayOf(0x4f, 0x4b), byteArrayOf(1, 2, 3, 4))) {
            ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
                server.soTimeout = 2000
                val executor = Executors.newSingleThreadExecutor()
                try {
                    val peer = executor.submit {
                        server.accept().use { socket ->
                            socket.soTimeout = 2000
                            DataInputStream(socket.getInputStream()).readFully(ByteArray(20))
                            socket.getOutputStream().write(reply)
                        }
                    }
                    MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 2000).use {
                        assertThrows(IOException::class.java) { it.connectAndHandshake() }
                    }
                    peer.get(3, TimeUnit.SECONDS)
                } finally {
                    executor.shutdownNow()
                }
            }
        }
    }

    @Test(timeout = 5000)
    fun silentReceiverTimesOutBeforeRecordingCanBegin() {
        ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
            MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 100).use {
                assertThrows(SocketTimeoutException::class.java) { it.connectAndHandshake() }
            }
        }
    }

    @Test(timeout = 5000)
    fun closingConnectionUnblocksAcknowledgementWait() {
        ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
            server.soTimeout = 2000
            val executor = Executors.newSingleThreadExecutor()
            val connection = MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 10000)
            try {
                val stopped = executor.submit<Boolean> {
                    try {
                        connection.connectAndHandshake()
                        false
                    } catch (_: IOException) {
                        true
                    }
                }
                server.accept().use { socket ->
                    socket.soTimeout = 2000
                    DataInputStream(socket.getInputStream()).readFully(ByteArray(20))
                    connection.close()
                    assertTrue(stopped.get(1, TimeUnit.SECONDS))
                }
            } finally {
                connection.close()
                executor.shutdownNow()
            }
        }
    }

    @Test(timeout = 10000)
    fun unresponsiveReceiverCannotQueueAudioIndefinitely() {
        ServerSocket().use { server ->
            server.receiveBufferSize = 1024
            server.bind(InetSocketAddress("127.0.0.1", 0))
            server.soTimeout = 2000
            val releasePeer = CountDownLatch(1)
            val executor = Executors.newSingleThreadExecutor()
            try {
                val peer = executor.submit {
                    server.accept().use { socket ->
                        socket.soTimeout = 2000
                        DataInputStream(socket.getInputStream()).readFully(ByteArray(20))
                        socket.getOutputStream().write(byteArrayOf(0x4f, 0x4b, 0x41, 0x59))
                        releasePeer.await(8, TimeUnit.SECONDS)
                    }
                }
                MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 500).use { connection ->
                    connection.connectAndHandshake()
                    val frame = ByteBuffer.wrap(ByteArray(1924).apply {
                        this[0] = 0x80.toByte()
                        this[1] = 7
                    })
                    assertThrows(SocketTimeoutException::class.java) {
                        repeat(10000) {
                            frame.rewind()
                            connection.writeFrame(frame)
                        }
                    }
                }
                releasePeer.countDown()
                peer.get(2, TimeUnit.SECONDS)
            } finally {
                releasePeer.countDown()
                executor.shutdownNow()
            }
        }
    }

    @Test(timeout = 5000)
    fun waitsForEachFrameAcknowledgementBeforeSendingNextFrame() {
        ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
            server.soTimeout = 2000
            val executor = Executors.newSingleThreadExecutor()
            val connection = MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 2000)
            try {
                val written = executor.submit {
                    connection.connectAndHandshake()
                    connection.writeFrame(ByteBuffer.wrap(byteArrayOf(2, 0, 0, 0, 1, 0)))
                    connection.writeFrame(ByteBuffer.wrap(byteArrayOf(2, 0, 0, 0, 2, 0)))
                }
                server.accept().use { socket ->
                    socket.soTimeout = 2000
                    val input = DataInputStream(socket.getInputStream())
                    input.readFully(ByteArray(20))
                    socket.getOutputStream().write(byteArrayOf(0x4f, 0x4b, 0x41, 0x59))
                    assertArrayEquals(byteArrayOf(2, 0, 0, 0, 1, 0),
                        ByteArray(6).also { input.readFully(it) })
                    socket.soTimeout = 150
                    assertThrows(SocketTimeoutException::class.java) { input.read() }
                    socket.getOutputStream().write(0x06)
                    socket.soTimeout = 2000
                    assertArrayEquals(byteArrayOf(2, 0, 0, 0, 2, 0),
                        ByteArray(6).also { input.readFully(it) })
                    assertThrows(TimeoutException::class.java) { written.get(150, TimeUnit.MILLISECONDS) }
                    socket.getOutputStream().write(0x06)
                    written.get(1, TimeUnit.SECONDS)
                }
            } finally {
                connection.close()
                executor.shutdownNow()
            }
        }
    }

    @Test(timeout = 10000)
    fun rejectsMissingAndIncorrectFrameAcknowledgements() {
        for (reply in listOf(byteArrayOf(), byteArrayOf(0), byteArrayOf(0x15), byteArrayOf(0x07))) {
            ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
                server.soTimeout = 2000
                val executor = Executors.newSingleThreadExecutor()
                try {
                    val peer = executor.submit {
                        server.accept().use { socket ->
                            socket.soTimeout = 2000
                            val input = DataInputStream(socket.getInputStream())
                            input.readFully(ByteArray(20))
                            socket.getOutputStream().write(byteArrayOf(0x4f, 0x4b, 0x41, 0x59))
                            input.readFully(ByteArray(6))
                            socket.getOutputStream().write(reply)
                        }
                    }
                    MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 2000).use {
                        it.connectAndHandshake()
                        assertThrows(IOException::class.java) {
                            it.writeFrame(ByteBuffer.wrap(byteArrayOf(2, 0, 0, 0, 1, 0)))
                        }
                    }
                    peer.get(2, TimeUnit.SECONDS)
                } finally {
                    executor.shutdownNow()
                }
            }
        }
    }

    @Test(timeout = 5000)
    fun missingFrameAcknowledgementTimesOut() {
        ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
            server.soTimeout = 2000
            val releasePeer = CountDownLatch(1)
            val executor = Executors.newSingleThreadExecutor()
            try {
                val peer = executor.submit {
                    server.accept().use { socket ->
                        socket.soTimeout = 2000
                        val input = DataInputStream(socket.getInputStream())
                        input.readFully(ByteArray(20))
                        socket.getOutputStream().write(byteArrayOf(0x4f, 0x4b, 0x41, 0x59))
                        input.readFully(ByteArray(6))
                        releasePeer.await(3, TimeUnit.SECONDS)
                    }
                }
                MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 150).use {
                    it.connectAndHandshake()
                    assertThrows(SocketTimeoutException::class.java) {
                        it.writeFrame(ByteBuffer.wrap(byteArrayOf(2, 0, 0, 0, 1, 0)))
                    }
                }
                releasePeer.countDown()
                peer.get(1, TimeUnit.SECONDS)
            } finally {
                releasePeer.countDown()
                executor.shutdownNow()
            }
        }
    }

    @Test(timeout = 5000)
    fun closingConnectionUnblocksFrameAcknowledgementWait() {
        ServerSocket(0, 1, InetAddress.getLoopbackAddress()).use { server ->
            server.soTimeout = 2000
            val executor = Executors.newSingleThreadExecutor()
            val connection = MicrophoneConnection(InetSocketAddress("127.0.0.1", server.localPort), 10000)
            try {
                val stopped = executor.submit<Boolean> {
                    try {
                        connection.connectAndHandshake()
                        connection.writeFrame(ByteBuffer.wrap(byteArrayOf(2, 0, 0, 0, 1, 0)))
                        false
                    } catch (_: IOException) {
                        true
                    }
                }
                server.accept().use { socket ->
                    socket.soTimeout = 2000
                    val input = DataInputStream(socket.getInputStream())
                    input.readFully(ByteArray(20))
                    socket.getOutputStream().write(byteArrayOf(0x4f, 0x4b, 0x41, 0x59))
                    input.readFully(ByteArray(6))
                    connection.close()
                    assertTrue(stopped.get(1, TimeUnit.SECONDS))
                }
            } finally {
                connection.close()
                executor.shutdownNow()
            }
        }
    }
}
