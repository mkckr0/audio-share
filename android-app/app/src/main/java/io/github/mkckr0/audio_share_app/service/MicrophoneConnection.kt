package io.github.mkckr0.audio_share_app.service

import java.io.Closeable
import java.io.EOFException
import java.io.IOException
import java.net.InetSocketAddress
import java.net.SocketTimeoutException
import java.nio.ByteBuffer
import java.nio.channels.CancelledKeyException
import java.nio.channels.ClosedChannelException
import java.nio.channels.ClosedSelectorException
import java.nio.channels.SelectionKey
import java.nio.channels.Selector
import java.nio.channels.SocketChannel
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

/** One bounded, cancellable connection. Closing it also wakes a pending selector. */
internal class MicrophoneConnection(
    private val address: InetSocketAddress,
    private val timeoutMillis: Long,
) : Closeable {
    private val closed = AtomicBoolean(false)
    private val selector = Selector.open()
    private val channel = try {
        SocketChannel.open().apply {
            configureBlocking(false)
            socket().tcpNoDelay = true
            socket().sendBufferSize = MicrophoneProtocol.MAX_FRAME_BYTES * 4
        }
    } catch (error: Exception) {
        selector.close()
        throw error
    }
    private val key = channel.register(selector, 0)
    private val frameAcknowledgement = ByteBuffer.allocate(1)

    fun connectAndHandshake() = io {
        val deadline = deadline()
        if (!channel.connect(address)) {
            while (!channel.finishConnect()) await(SelectionKey.OP_CONNECT, deadline)
        }
        writeFully(ByteBuffer.wrap(MicrophoneProtocol.header()))
        val acknowledgement = ByteBuffer.allocate(4)
        readFully(acknowledgement)
        MicrophoneProtocol.requireAcknowledgement(acknowledgement.array())
    }

    // One unacknowledged frame bounds buffering in both the socket and the ADB USB transport.
    @Synchronized
    fun writeFrame(frame: ByteBuffer) = io {
        writeFully(frame)
        frameAcknowledgement.clear()
        readFully(frameAcknowledgement)
        if (frameAcknowledgement.get(0) != 0x06.toByte()) {
            throw IOException("Microphone receiver rejected the audio frame")
        }
    }

    private fun readFully(bytes: ByteBuffer) {
        val readDeadline = deadline()
        while (bytes.hasRemaining()) {
            checkDeadline(readDeadline)
            when (channel.read(bytes)) {
                -1 -> throw EOFException("Receiver disconnected before acknowledgement")
                0 -> await(SelectionKey.OP_READ, readDeadline)
            }
        }
    }

    private fun writeFully(bytes: ByteBuffer) {
        val deadline = deadline()
        while (bytes.hasRemaining()) {
            checkDeadline(deadline)
            if (channel.write(bytes) == 0) await(SelectionKey.OP_WRITE, deadline)
        }
    }

    private fun await(operation: Int, deadline: Long) {
        key.interestOps(operation)
        while (true) {
            if (closed.get()) throw ClosedChannelException()
            val remainingNanos = checkDeadline(deadline)
            val waitMillis = TimeUnit.NANOSECONDS.toMillis(remainingNanos).coerceAtLeast(1)
            if (selector.select(waitMillis) > 0) {
                selector.selectedKeys().clear()
                return
            }
        }
    }

    private fun deadline() = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMillis)

    private fun checkDeadline(deadline: Long): Long {
        if (closed.get()) throw ClosedChannelException()
        val remaining = deadline - System.nanoTime()
        if (remaining <= 0) throw SocketTimeoutException("Microphone receiver timed out")
        return remaining
    }

    private inline fun <T> io(block: () -> T): T = try {
        block()
    } catch (error: ClosedSelectorException) {
        throw IOException("Microphone connection closed", error)
    } catch (error: CancelledKeyException) {
        throw IOException("Microphone connection closed", error)
    }

    override fun close() {
        if (closed.compareAndSet(false, true)) {
            try {
                channel.close()
            } finally {
                selector.close()
            }
        }
    }
}
