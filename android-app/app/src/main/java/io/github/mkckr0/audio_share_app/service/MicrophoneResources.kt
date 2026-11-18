package io.github.mkckr0.audio_share_app.service

import java.io.Closeable
import java.io.IOException

internal class MicrophoneResources : Closeable {
    private val lock = Any()
    private var closed = false
    private val resources = mutableListOf<Closeable>()

    fun <T : Closeable> add(resource: T): T {
        synchronized(lock) {
            if (!closed) {
                resources.add(resource)
                return resource
            }
        }
        closeQuietly(resource)
        throw IOException("Microphone session stopped")
    }

    fun release(resource: Closeable) {
        synchronized(lock) { resources.remove(resource) }
        closeQuietly(resource)
    }

    override fun close() {
        val toClose = synchronized(lock) {
            if (closed) return
            closed = true
            resources.toList().asReversed().also { resources.clear() }
        }
        toClose.forEach(::closeQuietly)
    }

    private fun closeQuietly(resource: Closeable) {
        try {
            resource.close()
        } catch (_: Exception) {
            // Always release the other resources, even after a device/connection error.
        }
    }
}
