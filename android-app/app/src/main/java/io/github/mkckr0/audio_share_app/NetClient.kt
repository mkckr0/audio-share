/**
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

package io.github.mkckr0.audio_share_app

import android.app.Application
import android.health.connect.datatypes.units.Volume
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.audiofx.LoudnessEnhancer
import android.os.Build
import android.util.Log
import androidx.preference.PreferenceManager
import io.github.mkckr0.audio_share_app.pb.*
import io.netty.bootstrap.Bootstrap
import io.netty.buffer.ByteBuf
import io.netty.channel.Channel
import io.netty.channel.ChannelHandler.Sharable
import io.netty.channel.ChannelHandlerContext
import io.netty.channel.ChannelInboundHandlerAdapter
import io.netty.channel.ChannelInitializer
import io.netty.channel.ChannelOption
import io.netty.channel.ConnectTimeoutException
import io.netty.channel.nio.NioEventLoopGroup
import io.netty.channel.socket.DatagramChannel
import io.netty.channel.socket.DatagramPacket
import io.netty.channel.socket.SocketChannel
import io.netty.channel.socket.nio.NioDatagramChannel
import io.netty.channel.socket.nio.NioSocketChannel
import io.netty.handler.codec.ByteToMessageDecoder
import io.netty.handler.codec.MessageToByteEncoder
import io.netty.handler.codec.MessageToMessageDecoder
import io.netty.handler.codec.MessageToMessageEncoder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import java.io.IOException
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.Timer
import kotlin.concurrent.timer
import kotlin.time.Duration.Companion.seconds
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

@Sharable
class NetClient(private val handler: Handler, private val application: Application) {

    private val tag = NetClient::class.simpleName
    private val sharedPreferences by lazy {
        PreferenceManager.getDefaultSharedPreferences(
            application
        )
    }

    interface Handler {
        fun onNetError(e: String)
        fun onAudioStop()
        fun onAudioStart()
    }

    enum class CMD {
        CMD_NONE,
        CMD_GET_FORMAT,
        CMD_START_PLAY,
        CMD_HEARTBEAT,
    }

    private var tcpChannel: Channel? = null
    private var udpChannel: Channel? = null
    private var heartbeatTimer: Timer? = null

    @OptIn(ExperimentalTime::class)
    private var heartbeatLastTick = TimeSource.Monotonic.markNow()
    private var _audioTrack: AudioTrack? = null
    private val audioTrack get() = _audioTrack!!
    private var _loudnessEnhancer: LoudnessEnhancer? = null
    private val loudnessEnhancer get() = _loudnessEnhancer!!

    fun start(host: String, port: Int) {
        MainScope().launch(Dispatchers.IO) {
            val workerGroup = NioEventLoopGroup()
            try {
                val connectTimeout = sharedPreferences.getString(
                    "audio_tcp_connect_timeout",
                    application.getString(R.string.audio_tcp_connect_timeout)
                )!!.toInt()
                val remoteAddress = InetSocketAddress(host, port)
                val f = Bootstrap().group(workerGroup)
                    .channel(NioSocketChannel::class.java)
                    .option(ChannelOption.TCP_NODELAY, true)
                    .option(ChannelOption.CONNECT_TIMEOUT_MILLIS, connectTimeout)
                    .handler(object : ChannelInitializer<SocketChannel>() {
                        override fun initChannel(ch: SocketChannel) {
                            ch.pipeline()
                                .addLast(TcpChannelAdapter.TcpMessageEncoder())
                                .addLast(TcpChannelAdapter.TcpMessageDecoder())
                                .addLast(TcpChannelAdapter(this@NetClient))
                        }
                    }).connect(remoteAddress).sync()

                if (!f.isSuccess) {
                    if (f.cause() != null) {
                        onFailed(f.cause())
                    }
                    return@launch
                }

                tcpChannel = f.channel()
                Log.d(tag, "tcpChannel.localAddress: ${tcpChannel!!.localAddress()}")

                udpChannel = Bootstrap().group(workerGroup)
                    .channel(NioDatagramChannel::class.java)
                    .handler(object : ChannelInitializer<DatagramChannel>() {
                        override fun initChannel(ch: DatagramChannel) {
                            ch.pipeline()
                                .addLast(UdpChannelAdapter.UdpMessageEncoder(remoteAddress))
                                .addLast(UdpChannelAdapter.UdpMessageDecoder())
                                .addLast(UdpChannelAdapter(this@NetClient))
                        }
                    }).bind((tcpChannel!!.localAddress() as InetSocketAddress).hostString, 0).sync()
                    .channel()

                Log.d(tag, "udpChannel.localAddress: ${udpChannel!!.localAddress()}")

                tcpChannel?.closeFuture()?.sync()
                udpChannel?.closeFuture()?.sync()
            } catch (e: Exception) {
                onFailed(e)
            } finally {
                workerGroup.shutdownGracefully()
            }
        }
    }

    class TcpChannelAdapter(private val parent: NetClient) : ChannelInboundHandlerAdapter() {
        companion object {
            private val tag = TcpChannelAdapter::class.simpleName
        }

        @Deprecated("Deprecated in Java")
        override fun exceptionCaught(ctx: ChannelHandlerContext?, cause: Throwable?) {
            if (cause != null) {
                Log.d(tag, cause.stackTraceToString())
            }
            parent.onFailed(cause)
        }

        override fun channelActive(ctx: ChannelHandlerContext) {
            Log.d(tag, "connected")
            parent.sendCMD(ctx, CMD.CMD_GET_FORMAT)
        }

        override fun channelInactive(ctx: ChannelHandlerContext) {
            Log.d(tag, "disconnected")
            if (parent.tcpChannel != null) {
                parent.onFailed(IOException("Connection close by peer"))
            }
        }

        class TcpMessage {
            var cmd: CMD = CMD.CMD_NONE
            var audioFormat: Client.AudioFormat? = null
            var id: Int = 0
        }

        @OptIn(ExperimentalTime::class)
        override fun channelRead(ctx: ChannelHandlerContext, msg: Any) {
            Log.d(tag, "channelRead tcp")
            try {
                if (msg is TcpMessage) {
                    if (msg.cmd == CMD.CMD_GET_FORMAT) {
                        val audioFormat = msg.audioFormat
                        if (audioFormat != null) {
                            parent.onGetFormat(ctx, audioFormat)
                        }
                    } else if (msg.cmd == CMD.CMD_START_PLAY) {
                        val id = msg.id
                        if (id > 0) {
                            parent.udpChannel?.writeAndFlush(id)
                            parent.heartbeatLastTick = TimeSource.Monotonic.markNow()
                            parent.heartbeatTimer = timer(null, false, 0, 3000) {
                                if (TimeSource.Monotonic.markNow() - parent.heartbeatLastTick > 5.seconds) {
                                    parent.onFailed(IOException("Heartbeat Timeout"))
                                }
                                Log.d(tag, "heartbeatTimer")
                            }
                            Log.d(tag, "start heartbeat")
                        } else {
                            Log.e(tag, "id <= 0")
                        }
                    } else if (msg.cmd == CMD.CMD_HEARTBEAT) {
                        parent.heartbeatLastTick = TimeSource.Monotonic.markNow()
                        parent.sendCMD(ctx, CMD.CMD_HEARTBEAT)
                    } else {
                        Log.e(tag, "error cmd")
                    }
                } else {
                    Log.e(tag, "msg is not valid type")
                }
            } catch (e: Exception) {
                Log.d("channelRead tcp", e.stackTraceToString())
            }
        }

        class TcpMessageDecoder : ByteToMessageDecoder() {
            override fun decode(
                ctx: ChannelHandlerContext?,
                `in`: ByteBuf?,
                out: MutableList<Any>?
            ) {
                if (`in` == null || out == null) {
                    return
                }

                Log.d(tag, "decode")

                if (`in`.readableBytes() < Int.SIZE_BYTES) {
                    return
                }

                `in`.markReaderIndex()

                val tcpMessage = TcpMessage()
                val id = `in`.readIntLE()

                Log.d(tag, "decode cmd id=${id}")

                val cmd = CMD.values()[id]

                tcpMessage.cmd = cmd

                if (cmd == CMD.CMD_GET_FORMAT) {
                    if (`in`.readableBytes() < Int.SIZE_BYTES) {
                        `in`.resetReaderIndex()
                        return
                    }

                    val bufSize = `in`.readIntLE()
                    if (`in`.readableBytes() < bufSize) {
                        `in`.resetReaderIndex()
                        return
                    }

                    val buf = ByteArray(bufSize)
                    `in`.readBytes(buf)
                    val format = Client.AudioFormat.parseFrom(buf)
                    tcpMessage.audioFormat = format

                } else if (cmd == CMD.CMD_START_PLAY) {
                    val x = `in`.readableBytes()
                    Log.d(tag, "readableBytes $x")
                    if (`in`.readableBytes() < Int.SIZE_BYTES) {
                        `in`.resetReaderIndex()
                        return
                    }
                    tcpMessage.id = `in`.readIntLE()
                }

                out.add(tcpMessage)
            }
        }

        open class TcpMessageEncoder : MessageToByteEncoder<Int>() {
            override fun encode(ctx: ChannelHandlerContext?, msg: Int?, out: ByteBuf?) {
                if (msg != null && out != null) {
                    out.writeIntLE(msg)
                }
            }
        }
    }

    class UdpChannelAdapter(private val parent: NetClient) : ChannelInboundHandlerAdapter() {

        private val tag = NetClient::class.simpleName

        @Deprecated("Deprecated in Java")
        override fun exceptionCaught(ctx: ChannelHandlerContext?, cause: Throwable?) {
            Log.d(tag, "exceptionCaught")
            parent.onFailed(cause)
        }

        class UdpMessageEncoder(private val remoteAddress: InetSocketAddress) :
            MessageToMessageEncoder<Int>() {
            override fun encode(ctx: ChannelHandlerContext?, msg: Int?, out: MutableList<Any>?) {
                if (ctx == null || msg == null || out == null) {
                    return
                }
                val buf = ctx.alloc().buffer(Int.SIZE_BYTES)
                buf.writeIntLE(msg)
                val data = DatagramPacket(buf, remoteAddress)
                out.add(data)
            }
        }

        class UdpMessageDecoder : MessageToMessageDecoder<DatagramPacket>() {
            override fun decode(
                ctx: ChannelHandlerContext?,
                msg: DatagramPacket?,
                out: MutableList<Any>?
            ) {
                if (ctx == null || msg == null || out == null) {
                    return
                }

//                Log.d("NetClient", "Udp decode")

                val buf = ByteBuffer.allocate(msg.content().readableBytes())
                msg.content().readBytes(buf)
                buf.flip()
                // keep PCM sample always be LE.
                val floatBuffer = buf.order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
                val floatArray = FloatArray(floatBuffer.capacity())
                floatBuffer.get(floatArray)
                out.add(floatArray)
            }
        }

        override fun channelRead(ctx: ChannelHandlerContext, msg: Any) {
//            Log.d(tag, "channelRead udp")
            try {
                if (msg is FloatArray) {
                    parent.audioTrack.write(
                        msg, 0, msg.size, AudioTrack.WRITE_NON_BLOCKING
                    )
                } else {
                    Log.e("NetClient", "msg is not valid type")
                }
            } catch (e: Exception) {
                Log.d("channelRead", e.stackTraceToString())
            }
        }
    }

    fun stop() {
        destroy()
        handler.onAudioStop()
    }

    fun setVolume(gain: Float) {
        audioTrack.setVolume(gain)
    }

    private fun destroy() {
        heartbeatTimer?.cancel()
        heartbeatTimer = null
        tcpChannel?.close()
        tcpChannel = null
        udpChannel?.close()
        udpChannel = null
        _loudnessEnhancer?.release()
        _loudnessEnhancer = null
        _audioTrack?.release()
        _audioTrack = null
    }

    fun isPlaying(): Boolean {
        return _audioTrack != null
    }

    private fun sendCMD(ctx: ChannelHandlerContext, cmd: CMD) {
        ctx.writeAndFlush(cmd.ordinal)
    }

    private fun onGetFormat(ctx: ChannelHandlerContext, format: Client.AudioFormat) {

        _audioTrack = createAudioTrack(format)

        MainScope().launch(Dispatchers.IO) {
            audioTrack.play()
        }

        MainScope().launch(Dispatchers.Main) {
            handler.onAudioStart()
        }

        // send start playing
        sendCMD(ctx, CMD.CMD_START_PLAY)
    }

    private fun createAudioTrack(format: Client.AudioFormat): AudioTrack {
        val encoding = when (format.formatTag) {
            0x0003 -> {
                AudioFormat.ENCODING_PCM_FLOAT
            }

            0x0001 -> {
                when (format.bitsPerSample) {
                    8 -> AudioFormat.ENCODING_PCM_8BIT
                    16 -> AudioFormat.ENCODING_PCM_16BIT
                    24 -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        AudioFormat.ENCODING_PCM_24BIT_PACKED
                    } else {
                        AudioFormat.ENCODING_INVALID
                    }

                    32 -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        AudioFormat.ENCODING_PCM_32BIT
                    } else {
                        AudioFormat.ENCODING_INVALID
                    }

                    else -> AudioFormat.ENCODING_INVALID
                }
            }

            else -> {
                AudioFormat.ENCODING_INVALID
            }
        }

        val channelMask = when (format.channels) {
            1 -> AudioFormat.CHANNEL_OUT_MONO
            2 -> AudioFormat.CHANNEL_OUT_STEREO
            3 -> AudioFormat.CHANNEL_OUT_STEREO or AudioFormat.CHANNEL_OUT_FRONT_CENTER
            4 -> AudioFormat.CHANNEL_OUT_QUAD
            5 -> AudioFormat.CHANNEL_OUT_QUAD or AudioFormat.CHANNEL_OUT_FRONT_CENTER
            6 -> AudioFormat.CHANNEL_OUT_5POINT1
            7 -> AudioFormat.CHANNEL_OUT_5POINT1 or AudioFormat.CHANNEL_OUT_BACK_CENTER
            8 -> AudioFormat.CHANNEL_OUT_7POINT1_SURROUND
            else -> AudioFormat.CHANNEL_INVALID
        }

        Log.i(
            tag,
            "encoding: $encoding, channelMask: $channelMask, sampleRate: ${format.sampleRate}"
        )

        Log.i(tag, "native order: ${ByteOrder.nativeOrder()}")

        val minBufferSize = AudioTrack.getMinBufferSize(format.sampleRate, channelMask, encoding)
        val bufferSizeScale = sharedPreferences.getString(
            "audio_buffer_size_scale",
            application.getString(R.string.audio_buffer_size_scale)
        )!!.toInt()
        val bufferSize = minBufferSize * bufferSizeScale
        Log.i(
            tag,
            "minBufferSize: $minBufferSize, bufferSizeScale: $bufferSizeScale, bufferSize: $bufferSize Bytes"
        )

        val builder = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(encoding)
                    .setChannelMask(channelMask)
                    .setSampleRate(format.sampleRate)
                    .build()
            ).setBufferSizeInBytes(bufferSize)
            .setTransferMode(AudioTrack.MODE_STREAM)

//        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
//            builder.setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
//        }

        val audioTrack = builder.build()

        _loudnessEnhancer = LoudnessEnhancer(audioTrack.audioSessionId)
        val targetGain = sharedPreferences.getString(
            "audio_loudness_enhancer",
            application.getString(R.string.audio_loudness_enhancer)
        )!!.toInt()
        loudnessEnhancer.setTargetGain(targetGain)
        loudnessEnhancer.enabled = true
        Log.d(tag, "LoudnessEnhancer targetGain: ${loudnessEnhancer.targetGain}mB")

        val audioVolume = sharedPreferences.getFloat(
            "audio_volume",
            AudioTrack.getMaxVolume()
        )
        audioTrack.setVolume(audioVolume)

        return audioTrack
    }

    private fun onFailed(exc: Throwable?) {
        if (exc == null) {
            return
        }

        MainScope().launch(Dispatchers.Main) {
            if (exc is ConnectTimeoutException || exc is IOException) {
                Log.d(tag, exc.message!!)
                handler.onNetError(exc.message!!)
                return@launch
            }
            Log.d(tag, exc.stackTraceToString())
            handler.onNetError(exc.stackTraceToString())
        }

        destroy()
    }
}