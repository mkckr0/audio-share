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

package io.github.mkckr0.audio_share_app.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Computer
import androidx.compose.material.icons.filled.PauseCircle
import androidx.compose.material.icons.filled.PlayCircle
import androidx.compose.material.icons.filled.WifiFind
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.core.text.isDigitsOnly
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.media3.common.Player
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.service.AudioPlayer
import io.github.mkckr0.audio_share_app.ui.MainActivity
import io.github.mkckr0.audio_share_app.ui.screen.HomeScreenViewModel.DiscoveryState
import io.github.mkckr0.audio_share_app.ui.screen.HomeScreenViewModel.UiState
import kotlinx.coroutines.launch

@Composable
fun HomeScreen(viewModel: HomeScreenViewModel = viewModel()) {
    val context = LocalContext.current
    val activity = context as MainActivity
    val scope = rememberCoroutineScope()

    when (val uiState = viewModel.uiState.collectAsStateWithLifecycle().value) {
        UiState.Loading -> {}
        is UiState.Success -> {
            var host by remember(uiState.host) { mutableStateOf(uiState.host) }
            var port by remember(uiState.port) { mutableStateOf(uiState.port.toString()) }
            var started by remember { mutableStateOf(false) }
            val isHostError by remember { derivedStateOf {
                host.isEmpty()
            } }
            val isPortError by remember { derivedStateOf {
                port.isEmpty()
            } }
            val discoveryState by viewModel.discoveryState.collectAsStateWithLifecycle()
            var serverMenuExpanded by remember { mutableStateOf(false) }

            fun discover() {
                port.toIntOrNull()?.let { viewModel.discoverServers(it) }
            }

            // search the local network on open, so the host doesn't need to be typed by hand
            LaunchedEffect(Unit) {
                if (discoveryState == DiscoveryState.Idle) {
                    discover()
                }
            }

            LaunchedEffect(discoveryState) {
                val servers = (discoveryState as? DiscoveryState.Done)?.servers ?: return@LaunchedEffect
                if (started || servers.isEmpty()) {
                    return@LaunchedEffect
                }
                if (servers.none { it.host == host }) {
                    host = servers.first().host
                }
                serverMenuExpanded = servers.size > 1
            }

            Column(
                modifier = Modifier
                    .padding(16.dp)
                    .fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(16.dp, Alignment.CenterVertically),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Row(
                    modifier = Modifier.weight(1f),
                    horizontalArrangement = Arrangement.spacedBy(
                        8.dp,
                        Alignment.CenterHorizontally
                    ),
                    verticalAlignment = Alignment.Bottom
                ) {
                    Box(modifier = Modifier.weight(0.7f)) {
                        OutlinedTextField(
                            value = host,
                            onValueChange = {
                                if (it.isEmpty() || !it.contains(Regex("\\s"))) {
                                    host = it
                                }
                            },
                            enabled = !started,
                            isError = isHostError,
                            label = { Text(context.getString(R.string.label_host)) },
                            trailingIcon = {
                                if (discoveryState == DiscoveryState.Scanning) {
                                    CircularProgressIndicator(
                                        modifier = Modifier.size(24.dp),
                                        strokeWidth = 2.dp,
                                    )
                                } else {
                                    IconButton(
                                        onClick = {
                                            val servers = (discoveryState as? DiscoveryState.Done)?.servers
                                            if (servers.isNullOrEmpty()) discover() else serverMenuExpanded = true
                                        },
                                        enabled = !started,
                                    ) {
                                        Icon(
                                            imageVector = Icons.Default.WifiFind,
                                            contentDescription = context.getString(R.string.label_find_servers),
                                        )
                                    }
                                }
                            },
                            supportingText = {
                                when (val state = discoveryState) {
                                    DiscoveryState.Idle -> {}
                                    DiscoveryState.Scanning -> Text(context.getString(R.string.label_searching_servers))
                                    is DiscoveryState.Done -> Text(
                                        if (state.servers.isEmpty()) context.getString(R.string.label_no_server_found)
                                        else context.resources.getQuantityString(
                                            R.plurals.label_servers_found, state.servers.size, state.servers.size
                                        )
                                    )
                                }
                            },
                            singleLine = true,
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                            modifier = Modifier.fillMaxWidth(),
                        )
                        DropdownMenu(
                            expanded = serverMenuExpanded && !started,
                            onDismissRequest = { serverMenuExpanded = false },
                        ) {
                            (discoveryState as? DiscoveryState.Done)?.servers?.forEach { server ->
                                DropdownMenuItem(
                                    text = { Text("${server.host}:${server.port}") },
                                    leadingIcon = { Icon(Icons.Default.Computer, contentDescription = null) },
                                    onClick = {
                                        host = server.host
                                        port = server.port.toString()
                                        serverMenuExpanded = false
                                    },
                                )
                            }
                            DropdownMenuItem(
                                text = { Text(context.getString(R.string.label_search_again)) },
                                leadingIcon = { Icon(Icons.Default.WifiFind, contentDescription = null) },
                                onClick = {
                                    serverMenuExpanded = false
                                    discover()
                                },
                            )
                        }
                    }
                    OutlinedTextField(
                        value = port,
                        onValueChange = {
                            if (it.isEmpty() || it.isDigitsOnly() && it.toInt() in 1..65535) {
                                port = it
                            }
                        },
                        enabled = !started,
                        isError = isPortError,
                        label = { Text(context.getString(R.string.label_port)) },
                        modifier = Modifier.weight(0.3f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    )
                }

                IconButton(
                    onClick = {
                        if (isHostError || isPortError) {
                            return@IconButton
                        }
                        scope.launch {
                            if (started) {
                                activity.awaitMediaController().stop()
                            } else {
                                try {
                                    viewModel.saveNetWorkSettings(host, port.toInt()).join()
                                    activity.awaitMediaController().play()
                                } catch (_: NumberFormatException) {
                                    return@launch
                                }
                            }
                        }
                    },
                    modifier = Modifier.size(80.dp),
                ) {
                    Icon(
                        imageVector = if (started) Icons.Default.PauseCircle else Icons.Default.PlayCircle,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.fillMaxSize()
                    )
                }

                Row(
                    modifier = Modifier.weight(1f)
                ) {
                    OutlinedCard(
                        modifier = Modifier.fillMaxSize()
                    ) {
                        SelectionContainer {
                            Text(
                                text = AudioPlayer.message,
                                modifier = Modifier
                                    .fillMaxSize()
                                    .padding(16.dp)
                                    .verticalScroll(rememberScrollState())
                            )
                        }
                    }
                }
            }

            LifecycleStartEffect(true) {
                val listener = object : Player.Listener {
                    override fun onPlayWhenReadyChanged(playWhenReady: Boolean, reason: Int) {
                        started = playWhenReady
                    }
                }

                scope.launch {
                    activity.awaitMediaController().run {
                        started = playWhenReady
                        addListener(listener)
                    }
                }

                onStopOrDispose {
                    scope.launch {
                        activity.awaitMediaController().removeListener(listener)
                    }
                }
            }
        }
    }
}
