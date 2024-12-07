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

import io.github.mkckr0.audio_share_app.pb.Client.AudioFormat
import io.github.mkckr0.audio_share_app.service.NetClient.CMD
import io.ktor.network.sockets.ConnectedDatagramSocket
import io.ktor.network.sockets.Datagram
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

suspend fun ByteReadChannel.readAudioFormat(): AudioFormat? {
    val size = readIntLE()
    return AudioFormat.parseFrom(readByteBuffer(size))
}

suspend fun ConnectedDatagramSocket.writeIntLE(value: Int) {
    send(Datagram(Buffer().apply {
        this.writeIntLe(value)
    }.build(), remoteAddress))
}

suspend fun ConnectedDatagramSocket.readByteBuffer(): ByteBuffer {
    return ByteBuffer.wrap(receive().packet.readByteArray())
}