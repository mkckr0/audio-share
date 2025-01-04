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

import android.content.Context
import android.util.Log
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.pb.Client.AudioFormat
import io.ktor.network.selector.SelectorManager
import io.ktor.network.sockets.BoundDatagramSocket
import io.ktor.network.sockets.ConnectedDatagramSocket
import io.ktor.network.sockets.InetSocketAddress
import io.ktor.network.sockets.Socket
import io.ktor.network.sockets.aSocket
import io.ktor.network.sockets.openReadChannel
import io.ktor.network.sockets.openWriteChannel
import io.ktor.network.sockets.toJavaAddress
import io.ktor.util.network.address
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineName
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.UnresolvedAddressException
import kotlin.time.Duration.Companion.seconds
import kotlin.time.TimeSource

class NetClient(val context: Context) {

    private val tag = NetClient::class.simpleName

    private fun defaultScope(): CoroutineScope = CoroutineScope(
        SupervisorJob() + Dispatchers.IO + CoroutineName("NetClientCoroutine") + CoroutineExceptionHandler { _, cause ->
            Log.d(tag, cause.stackTraceToString())
            _callback?.launch {
                onError(cause.message, cause)
            }
        }
    )

    private var _callback: Callback? = null
    private var _scope: CoroutineScope? = null
    private val scope: CoroutineScope get() = _scope!!

    private var _selectorManager: SelectorManager? = null
    private val selectorManager get() = _selectorManager!!
    private var _tcpSocket: Socket? = null
    private val tcpSocket get() = _tcpSocket!!
//    private var _udpSocket: ConnectedDatagramSocket? = null
    private var _udpSocket: BoundDatagramSocket? = null
    private val udpSocket get() = _udpSocket!!

    private var _heartbeatLastTick = TimeSource.Monotonic.markNow()

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

    fun start(host: String, port: Int, callback: Callback) {
        Log.d(tag, "$host:$port")
        _scope = defaultScope()
        scope.launch {
            _callback = callback

            if (_selectorManager != null) {
                throw Exception("Repeat start")
            }

            _callback?.launch {
                log("${context.getString(R.string.label_connecting)} $host:$port")
            }
            _selectorManager = SelectorManager(Dispatchers.IO)

            try {
                _tcpSocket = withTimeout(3.seconds) {
                    aSocket(selectorManager).tcp().connect(host, port)
                }
            } catch (e: TimeoutCancellationException) {
                throw Exception(context.getString(R.string.label_timeout))
            } catch (e: UnresolvedAddressException) {
                throw Exception(context.getString(R.string.label_unresolved_address))
            }

            _callback?.launch {
                log("TCP connected")
            }

            val tcpReadChannel = tcpSocket.openReadChannel()
            val tcpWriteChannel = tcpSocket.openWriteChannel()

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

//            _udpSocket = aSocket(selectorManager).udp()
//                .connect(InetSocketAddress(host, port))
            _udpSocket = aSocket(selectorManager).udp()
                .bind(InetSocketAddress(tcpSocket.localAddress.toJavaAddress().address, 0))

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
                udpSocket.writeIntLE(id, InetSocketAddress(host, port))
//                udpSocket.writeIntLE(id)
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
        Log.d(tag, "stop")
        _callback?.scope?.cancel()
        _callback = null
        _scope?.cancel()
        _scope = null
        _selectorManager?.close()
        _selectorManager = null
        _udpSocket?.close()
        _tcpSocket?.close()
    }
}