package io.github.mkckr0.audio_share_app

import Client
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.os.Build
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import java.net.ConnectException
import java.net.InetSocketAddress
import java.net.StandardSocketOptions
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.AsynchronousSocketChannel
import java.nio.channels.ClosedChannelException
import java.nio.channels.CompletionHandler

class NetClient(private val handler: Handler) {

    interface Handler {
        fun onNetError(e: String)
        fun onAudioStop()
        fun onAudioStart()
    }

    abstract class WriteCompletionHandler(private val self: NetClient): CompletionHandler<Int, ByteBuffer>
    {
        override fun completed(result: Int?, attachment: ByteBuffer?) {
            if (result == -1) {
                return
            }

            if (attachment != null) {
                if (attachment.remaining() > 0) {
                    self.channel.write(attachment, attachment, this)
                } else {
                    attachment.flip()
                    onCompleted(attachment)
                }
            }
        }

        override fun failed(exc: Throwable?, attachment: ByteBuffer?) {
            self.onFailed(exc)
        }

        abstract fun onCompleted(buffer: ByteBuffer)
    }

    abstract class ReadCompletionHandler(private val self: NetClient): CompletionHandler<Int, ByteBuffer>
    {
        override fun completed(result: Int?, attachment: ByteBuffer?) {

            if (result == -1) {
                return
            }

            if (attachment != null) {
                if (attachment.remaining() > 0) {
                    self.channel.read(attachment, attachment, this)
                } else {
                    attachment.flip()
                    onCompleted(attachment)
                }
            }
        }

        override fun failed(exc: Throwable?, attachment: ByteBuffer?) {
            self.onFailed(exc)
        }

        abstract fun onCompleted(buffer: ByteBuffer)
    }

    enum class CMD {
        CMD_NONE,
        CMD_GET_FORMAT,
        CMD_START_PLAY,
    }

    private lateinit var channel:AsynchronousSocketChannel
    private var audioTrack: AudioTrack? = null

    fun start(host: String, port: Int) {
        MainScope().launch(Dispatchers.IO) {
            try {
                channel = AsynchronousSocketChannel.open()
                channel.setOption(StandardSocketOptions.SO_KEEPALIVE, true)
                channel.setOption(StandardSocketOptions.TCP_NODELAY, true)
                val endpoint = InetSocketAddress(host, port)
                channel.connect(endpoint, null, object: CompletionHandler<Void, Void?> {
                    override fun completed(result: Void?, attachment: Void?) {
                        onConnected()
                    }

                    override fun failed(exc: Throwable?, attachment: Void?) {
                        onFailed(exc)
                    }
                })

                // check timeout
                Thread.sleep(1000)
                if (channel.isOpen && channel.remoteAddress == null) {
                    throw ConnectException("Connection timed out")
                }
            } catch (e: Exception) {
                onFailed(e)
            }
        }
    }

    fun stop() {
        channel.shutdownOutput()
        channel.shutdownInput()
        destroy()
        handler.onAudioStop()
    }

    private fun destroy() {
        channel.close()
        audioTrack = null
    }

    private fun onConnected() {
        sendCMD(CMD.CMD_GET_FORMAT)
    }

    private fun sendCMD(cmd: CMD) {
        val buf = ByteBuffer.allocate(Int.SIZE_BYTES).order(ByteOrder.LITTLE_ENDIAN)
        buf.putInt(cmd.ordinal).flip()
        channel.write(buf, buf, object : WriteCompletionHandler(this) {
            override fun onCompleted(buffer: ByteBuffer) {
                readCMD()
            }
        })
    }

    private fun readCMD() {
        val buf = ByteBuffer.allocate(Int.SIZE_BYTES).order(ByteOrder.LITTLE_ENDIAN)
        channel.read(buf, buf, object : ReadCompletionHandler(this) {
            override fun onCompleted(buffer: ByteBuffer) {
                val id = buffer.int
//                Log.d("AudioShare recv cmd", id.toString())
                onReadCMD(CMD.values()[id])
            }
        })
    }

    private fun onReadCMD(cmd: CMD) {
        if (cmd == CMD.CMD_GET_FORMAT) {
            // read format size
            val bufSize = ByteBuffer.allocate(Int.SIZE_BYTES).order(ByteOrder.LITTLE_ENDIAN)
            channel.read(bufSize, bufSize, object : ReadCompletionHandler(this) {
                override fun onCompleted(buffer: ByteBuffer) {
                    // read format
                    val buf = ByteBuffer.allocate(buffer.int)
                    channel.read(buf, buf, object : ReadCompletionHandler(this@NetClient) {
                        override fun onCompleted(buffer: ByteBuffer) {
                            onGetFormat(Client.AudioFormat.parseFrom(buf))
                        }
                    })
                }
            })

        } else if (cmd == CMD.CMD_START_PLAY) {
            // read audio data size
            val bufSize = ByteBuffer.allocate(Int.SIZE_BYTES).order(ByteOrder.LITTLE_ENDIAN)
            channel.read(bufSize, bufSize, object : ReadCompletionHandler(this) {
                override fun onCompleted(buffer: ByteBuffer) {
                    val buf = ByteBuffer.allocate(buffer.int)
                    // read audio data
                    channel.read(buf, buf, object : ReadCompletionHandler(this@NetClient) {
                        override fun onCompleted(buffer: ByteBuffer) {
                            val floatBuffer = buffer.order(ByteOrder.nativeOrder()).asFloatBuffer()
                            val floatArray = FloatArray(floatBuffer.capacity())
                            floatBuffer.get(floatArray)
                            audioTrack?.write(
                                floatArray, 0, floatArray.size, AudioTrack.WRITE_NON_BLOCKING
                            )
                            readCMD()
                        }
                    })
                }
            })
        }
    }

    private fun onGetFormat(format: Client.AudioFormat) {

        audioTrack = createAudioTrack(format)

        MainScope().launch(Dispatchers.IO) {
            audioTrack?.play()
        }

        handler.onAudioStart()

        // send start playing
        sendCMD(CMD.CMD_START_PLAY)
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

        val minBufferSize = AudioTrack.getMinBufferSize(format.sampleRate, channelMask, encoding)

        return AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(encoding)
                    .setChannelMask(channelMask)
                    .setSampleRate(format.sampleRate)
                    .build()
            ).setBufferSizeInBytes(minBufferSize)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
            .build()
    }

    private fun onFailed(exc: Throwable?) {
        if (exc == null) {
            return
        }

        if (exc is ClosedChannelException) {
            return
        }

        MainScope().launch(Dispatchers.Main) {
            val e = exc.message
            if (e != null && exc.message != "") {
//                handler.onNetError(e)
                handler.onNetError(e + "\n" + exc.stackTraceToString())
                return@launch
            }
            handler.onNetError(exc.stackTraceToString())
        }

        destroy()
    }
}