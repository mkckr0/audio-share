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

import android.util.Log
import io.github.mkckr0.audio_share_app.pb.Client.AudioFormat
import io.ktor.network.selector.SelectorManager
import io.ktor.network.sockets.BoundDatagramSocket
import io.ktor.network.sockets.Datagram
import io.ktor.network.sockets.DatagramReadChannel
import io.ktor.network.sockets.DatagramWriteChannel
import io.ktor.network.sockets.InetSocketAddress
import io.ktor.network.sockets.Socket
import io.ktor.network.sockets.aSocket
import io.ktor.network.sockets.openReadChannel
import io.ktor.network.sockets.openWriteChannel
import io.ktor.network.sockets.toJavaAddress
import io.ktor.util.network.address
import io.ktor.utils.io.ByteReadChannel
import io.ktor.utils.io.ByteWriteChannel
import io.ktor.utils.io.core.build
import io.ktor.utils.io.readByteArray
import io.ktor.utils.io.readPacket
import io.ktor.utils.io.writePacket
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineName
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import kotlinx.io.Buffer
import kotlinx.io.readByteArray
import kotlinx.io.readIntLe
import kotlinx.io.writeIntLe
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.Timer
import kotlin.concurrent.timer
import kotlin.time.Duration.Companion.seconds
import kotlin.time.TimeSource

class NetClient {

    private val tag = NetClient::class.simpleName

    private var _callback: Callback? = null
    private var _selectorManager: SelectorManager? = null
    private val selectorManager get() = _selectorManager!!
    private var _tcpSocket: Socket? = null
    private val tcpSocket get() = _tcpSocket!!
    private var _udpSocket: BoundDatagramSocket? = null
    private val udpSocket get() = _udpSocket!!

    private var _heartbeatLastTick = TimeSource.Monotonic.markNow()
    private var _heartbeatTimer: Timer? = null

    private var _host: String = ""
    private var _port: Int = 0

    private val scope = CoroutineScope(
        SupervisorJob() + Dispatchers.IO + CoroutineName("NetClientCoroutine") + CoroutineExceptionHandler { _, cause ->
            Log.d(tag, cause.stackTraceToString())
            _callback?.launch {
                log(cause.message ?: cause.stackTraceToString())
                onError(cause.message, cause)
            }
        }
    )

    enum class CMD {
        CMD_NONE,
        CMD_GET_FORMAT,
        CMD_START_PLAY,
        CMD_HEARTBEAT,
    }

    interface Callback {
        val scope: CoroutineScope
        suspend fun log(message: String)
        suspend fun onReceiveAudioFormat(format: AudioFormat)
        suspend fun onPlaybackStarted()
        suspend fun onReceiveAudioData(audioData: ByteBuffer)
        suspend fun onError(message: String?, cause: Throwable?)

        fun launch(block: suspend Callback.() -> Unit): Job {
            return scope.launch {
                block()
            }
        }
    }

    fun configure(host: String, port: Int, callback: Callback? = null) {
        Log.d(tag, "configure $host:$port")
        _host = host
        _port = port
        _callback = callback
    }

    private suspend fun ByteWriteChannel.writeCMD(cmd: CMD) {
        writePacket(Buffer().apply {
            writeIntLe(cmd.ordinal)
        }.build())
        flush()
    }

    private suspend fun ByteReadChannel.readByteBuffer(count: Int): ByteBuffer {
        return ByteBuffer.wrap(readByteArray(count))
    }

    private suspend fun ByteReadChannel.readIntLE(): Int {
        return readPacket(Int.SIZE_BYTES).readIntLe()
    }

    private suspend fun ByteReadChannel.readCMD(): CMD {
        return CMD.entries[readIntLE()]
    }

    private suspend fun ByteReadChannel.readAudioFormat(): AudioFormat? {
        val size = readIntLE()
        return AudioFormat.parseFrom(readByteBuffer(size))
    }

    private suspend fun DatagramWriteChannel.writeIntLE(value: Int) {
        send(Datagram(Buffer().apply {
            this.writeIntLe(value)
        }.build(), InetSocketAddress(_host, _port)))
    }

    private suspend fun DatagramReadChannel.readByteBuffer(): ByteBuffer {
        return ByteBuffer.wrap(receive().packet.readByteArray())
    }

    fun start() {
        scope.launch {
            Log.d(tag, "connecting $_host:$_port")
            _callback?.launch {
                log("connecting $_host:$_port")
            }
            _selectorManager = SelectorManager(Dispatchers.IO)

            try {
                _tcpSocket = withTimeout(3.seconds) {
                    aSocket(selectorManager).tcp().connect(_host, _port)
                }
            } catch (e: TimeoutCancellationException) {
                throw Exception("connect timeout", e)
            }

            Log.d(tag, "tcp connected")
            _callback?.launch {
                log("tcp connected")
            }

            val tcpReadChannel = tcpSocket.openReadChannel()
            val tcpWriteChannel = tcpSocket.openWriteChannel()

            _udpSocket = aSocket(selectorManager).udp()
                .bind(InetSocketAddress(tcpSocket.localAddress.toJavaAddress().address, 0))

            // get format
            tcpWriteChannel.writeCMD(CMD.CMD_GET_FORMAT)
            var cmd = tcpReadChannel.readCMD()
            if (cmd != CMD.CMD_GET_FORMAT) {
                return@launch
            }
            val audioFormat = tcpReadChannel.readAudioFormat() ?: return@launch
            _callback?.launch {
                onReceiveAudioFormat(audioFormat)
            }?.join()   // wait AudioTrack created

            _callback?.launch {
                log("get format success")
            }

            // start play
            tcpWriteChannel.writeCMD(CMD.CMD_START_PLAY)
            cmd = tcpReadChannel.readCMD()
            if (cmd != CMD.CMD_START_PLAY) {
                return@launch
            }
            val id = tcpReadChannel.readIntLE()
            if (id <= 0) {
                return@launch
            }

            _callback?.launch {
                onPlaybackStarted()
            }

            // heartbeat loop
            scope.launch {
                _heartbeatLastTick = TimeSource.Monotonic.markNow()
                while (true) {
                    Log.d(tag, "check heartbeat")
                    if (TimeSource.Monotonic.markNow() - _heartbeatLastTick > 5.seconds) {
                        throw Exception("heartbeat timeout")
                    }
                    delay(3.seconds)
                }
            }
            scope.launch {
                while (true) {
                    cmd = tcpReadChannel.readCMD()
                    if (cmd == CMD.CMD_HEARTBEAT) {
                        Log.d(tag, "receive heartbeat")
                        _heartbeatLastTick = TimeSource.Monotonic.markNow()
                        tcpWriteChannel.writeCMD(CMD.CMD_HEARTBEAT)
                    }
                }
            }

            // audio data read loop
            scope.launch {
                udpSocket.writeIntLE(id)
                while (true) {
                    val buf = udpSocket.readByteBuffer()
                    _callback?.launch {
                        onReceiveAudioData(buf.order(ByteOrder.LITTLE_ENDIAN))
                    }
                }
            }
        }
    }

    fun stop() {
        scope.coroutineContext.cancelChildren()
        _heartbeatTimer?.cancel()
        _heartbeatTimer = null
        _udpSocket?.close()
        _udpSocket = null
        _tcpSocket?.close()
        _tcpSocket = null
        _selectorManager?.close()
        _selectorManager = null
    }
}