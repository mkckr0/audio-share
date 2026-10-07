package io.github.mkckr0.audio_share_app.service

import android.media.AudioTrack
import android.os.Build
import android.os.Process
import android.os.SystemClock
import android.util.Log
import java.nio.ByteBuffer
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong

class AudioPipeline(
    private val track: AudioTrack,
    private val decoder: OpusDecoder?,
    private val bytesPerFrame: Int,
    maxQueued: Int,
    private val onStats: (String) -> Unit,
) : Thread("AudioShare-Playback") {

    private val tag = AudioPipeline::class.simpleName

    private val queue = ArrayBlockingQueue<ByteBuffer>(maxQueued)
    private val overflowDrops = AtomicLong()
    private val sequence = SequenceTracker()

    @Volatile
    private var running = true

    private val prebufferFrames = maxOf(1, track.bufferSizeInFrames / 2)
    private var framesWritten = 0L
    private var started = false
    private var lastStatsAt = 0L

    fun submit(packet: ByteBuffer) {
        // drop the oldest so a slow phone clock can't grow the latency forever
        while (!queue.offer(packet)) {
            if (queue.poll() != null) {
                overflowDrops.incrementAndGet()
            }
        }
    }

    fun shutdown() {
        running = false
        interrupt()
        join(1000)
    }

    override fun run() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
        try {
            while (running) {
                val packet = queue.poll(100, TimeUnit.MILLISECONDS)
                if (packet != null) {
                    handle(packet)
                }
                reportStats()
            }
        } catch (e: InterruptedException) {
        } catch (e: Exception) {
            Log.e(tag, e.stackTraceToString())
        } finally {
            decoder?.release()
        }
    }

    private fun handle(packet: ByteBuffer) {
        if (decoder == null) {
            write(packet, packet.remaining())
            return
        }

        if (packet.remaining() <= SEQ_BYTES) {
            return
        }
        val seq = (packet.get().toInt() and 0xff) or ((packet.get().toInt() and 0xff) shl 8)
        if (!sequence.accept(seq)) {
            return
        }
        decoder.decode(packet) { pcm, size -> write(pcm, size) }
    }

    private fun write(buffer: ByteBuffer, size: Int) {
        var remaining = size
        while (remaining > 0 && running) {
            // blocking before play() would deadlock if the buffer is smaller than one chunk
            val mode = if (started) AudioTrack.WRITE_BLOCKING else AudioTrack.WRITE_NON_BLOCKING
            val written = track.write(buffer, remaining, mode)
            if (written < 0) {
                Log.w(tag, "AudioTrack.write failed: $written")
                return
            }
            remaining -= written
            framesWritten += written / bytesPerFrame
            if (!started && (written == 0 || framesWritten >= prebufferFrames)) {
                track.play()
                started = true
            }
        }
    }

    private fun reportStats() {
        val now = SystemClock.elapsedRealtime()
        if (now - lastStatsAt < STATS_INTERVAL_MS) {
            return
        }
        lastStatsAt = now

        val parts = mutableListOf<String>()
        if (decoder != null) {
            parts += "Opus"
            parts += "lost ${sequence.lost}"
            parts += "late ${sequence.late}"
        } else {
            parts += "PCM"
        }
        parts += "dropped ${overflowDrops.get()}"
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            parts += "underruns ${track.underrunCount}"
        }
        if (running) {
            onStats(parts.joinToString(" · "))
        }
    }

    companion object {
        private const val SEQ_BYTES = 2
        private const val STATS_INTERVAL_MS = 2000L
    }
}
