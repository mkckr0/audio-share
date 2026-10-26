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

import io.github.mkckr0.audio_share_app.service.NetClient.CMD
import io.ktor.network.selector.SelectorManager
import io.ktor.network.sockets.aSocket
import io.ktor.network.sockets.openReadChannel
import io.ktor.network.sockets.openWriteChannel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import java.net.Inet4Address
import java.net.NetworkInterface
import kotlin.time.Duration.Companion.milliseconds

/**
 * Finds audio-share servers on the local networks of this device.
 *
 * Every IPv4 subnet the device is attached to (Wi-Fi, hotspot, ethernet) is scanned on the given port,
 * and each open port is verified with the [CMD.CMD_GET_FORMAT] handshake, so only real servers are reported.
 * No server-side support is needed.
 */
object ServerDiscovery {

    data class Server(
        val host: String,
        val port: Int,
        val interfaceName: String,
    )

    private data class LocalAddress(
        val address: Int,
        val prefixLength: Int,
        val interfaceName: String,
    )

    /** Scan at most a /22 around each local address, a /16 would take too long. */
    private const val MIN_PREFIX_LENGTH = 22
    private const val MAX_CONCURRENCY = 128
    private val connectTimeout = 400.milliseconds
    private val handshakeTimeout = 800.milliseconds

    suspend fun discover(port: Int): List<Server> = withContext(Dispatchers.IO) {
        val localAddresses = getLocalAddresses()
        val selfHosts = localAddresses.map { intToHost(it.address) }.toSet()
        val candidates = localAddresses
            .flatMap { local -> subnetHosts(local.address, local.prefixLength).map { it to local.interfaceName } }
            .distinctBy { it.first }
            .filter { it.first !in selfHosts }

        SelectorManager(Dispatchers.IO).use { selectorManager ->
            val semaphore = Semaphore(MAX_CONCURRENCY)
            coroutineScope {
                candidates.map { (host, interfaceName) ->
                    async {
                        semaphore.withPermit {
                            if (probe(selectorManager, host, port)) Server(host, port, interfaceName) else null
                        }
                    }
                }.awaitAll().filterNotNull()
            }
        }
    }

    /** Returns true if an audio-share server answers the get-format handshake on [host]:[port]. */
    suspend fun probe(selectorManager: SelectorManager, host: String, port: Int): Boolean {
        val socket = withTimeoutOrNull(connectTimeout) {
            runCatching { aSocket(selectorManager).tcp().connect(host, port) }.getOrNull()
        } ?: return false
        return socket.use {
            withTimeoutOrNull(handshakeTimeout) {
                runCatching {
                    val writeChannel = it.openWriteChannel()
                    val readChannel = it.openReadChannel()
                    writeChannel.writeCMD(CMD.CMD_GET_FORMAT)
                    readChannel.readCMD() == CMD.CMD_GET_FORMAT
                }.getOrDefault(false)
            } ?: false
        }
    }

    private fun getLocalAddresses(): List<LocalAddress> {
        val interfaces = runCatching { NetworkInterface.getNetworkInterfaces()?.toList() }.getOrNull() ?: return emptyList()
        return interfaces
            .filter { runCatching { it.isUp && !it.isLoopback && !it.isPointToPoint }.getOrDefault(false) }
            .flatMap { networkInterface ->
                networkInterface.interfaceAddresses.mapNotNull { interfaceAddress ->
                    val address = interfaceAddress.address as? Inet4Address ?: return@mapNotNull null
                    if (!address.isSiteLocalAddress) {
                        return@mapNotNull null
                    }
                    LocalAddress(
                        address = address.address.fold(0) { acc, byte -> (acc shl 8) or (byte.toInt() and 0xff) },
                        prefixLength = interfaceAddress.networkPrefixLength.toInt(),
                        interfaceName = networkInterface.displayName ?: networkInterface.name,
                    )
                }
            }
    }

    internal fun subnetHosts(address: Int, prefixLength: Int): List<String> {
        val prefix = prefixLength.coerceIn(MIN_PREFIX_LENGTH, 30)
        val mask = -1 shl (32 - prefix)
        val network = address and mask
        val broadcast = network or mask.inv()
        return ((network + 1) until broadcast).map { intToHost(it) }
    }

    internal fun intToHost(address: Int): String =
        "${address ushr 24 and 0xff}.${address ushr 16 and 0xff}.${address ushr 8 and 0xff}.${address and 0xff}"
}
