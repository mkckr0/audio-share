package com.audiostream.app.service

import com.audiostream.app.pb.AudioFormat as PbAudioFormat
import com.audiostream.app.service.NetClient.CMD
import io.ktor.network.sockets.BoundDatagramSocket
import io.ktor.network.sockets.ConnectedDatagramSocket
import io.ktor.network.sockets.Datagram
import io.ktor.network.sockets.DatagramReadChannel
import io.ktor.network.sockets.DatagramWriteChannel
import io.ktor.network.sockets.SocketAddress
import io.ktor.utils.io.ByteReadChannel
import io.ktor.utils.io.ByteWriteChannel
import io.ktor.utils.io.core.build
import io.ktor.utils.io.readByteArray
import io.ktor.utils.io.readPacket
import io.ktor.utils.io.writePacket
import kotlinx.io.Buffer
import kotlinx.io.readByteArray
import kotlinx.io.readIntLe
import kotlinx.io.writeIntLe
import java.nio.ByteBuffer

suspend fun ByteWriteChannel.writeCMD(cmd: CMD) {
    writePacket(Buffer().apply {
        writeIntLe(cmd.ordinal)
    }.build())
    flush()
}

suspend fun ByteReadChannel.readByteBuffer(count: Int): ByteBuffer {
    return ByteBuffer.wrap(readByteArray(count))
}

suspend fun ByteReadChannel.readIntLE(): Int {
    return readPacket(Int.SIZE_BYTES).readIntLe()
}

suspend fun ByteReadChannel.readCMD(): CMD {
    return CMD.entries[readIntLE()]
}

suspend fun ByteReadChannel.readAudioFormat(): PbAudioFormat? {
    val size = readIntLE()
    return PbAudioFormat.parseFrom(readByteBuffer(size))
}

suspend fun ConnectedDatagramSocket.writeIntLE(value: Int) {
    send(Datagram(Buffer().apply {
        this.writeIntLe(value)
    }.build(), remoteAddress))
}

suspend fun DatagramWriteChannel.writeIntLE(value: Int, address: SocketAddress) {
    send(Datagram(Buffer().apply {
        this.writeIntLe(value)
    }.build(), address))
}

suspend fun DatagramReadChannel.readByteBuffer(): ByteBuffer {
    return ByteBuffer.wrap(receive().packet.readByteArray())
}

