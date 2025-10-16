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
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.PauseCircleOutline
import androidx.compose.material.icons.outlined.PlayCircleOutline
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.core.text.isDigitsOnly
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.media3.common.Player
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.service.AudioPlayer
import io.github.mkckr0.audio_share_app.ui.MainActivity
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
            val isHostError by remember {
                derivedStateOf {
                    host.isEmpty() || !Regex("(([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])")
                        .matches(host) // regex to check correct ipv4 address
                }
            }
            val isPortError by remember {
                derivedStateOf {
                    port.isEmpty() || if (port.isDigitsOnly()) port.toInt() !in 1..65535 else true
                }
            }

            Column(
                modifier = Modifier
                    .padding(16.dp)
                    .fillMaxSize(),
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.weight(1f)
                ) {
                        Text(
                            text = AudioPlayer.message,
                            modifier = Modifier
                                .fillMaxWidth()
                                .verticalScroll(rememberScrollState()),
                            textAlign = TextAlign.Center
                        )
                }
                Row(
                    modifier = Modifier
                        .padding(4.dp, 0.dp),
                    horizontalArrangement = Arrangement.spacedBy(
                        8.dp,
                        Alignment.CenterHorizontally
                    )
                ) {
                    OutlinedTextField(
                        value = host,
                        onValueChange = {
                            host = it
                        },
                        enabled = !started,
                        isError = isHostError,
                        label = { Text(context.getString(R.string.label_host)) },
                        textStyle = TextStyle(textAlign = TextAlign.Center),
                        modifier = Modifier.weight(0.5f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        shape = RoundedCornerShape(25)
                    )
                    OutlinedTextField(
                        value = port,
                        onValueChange = {
                            port = it
                        },
                        enabled = !started,
                        isError = isPortError,
                        label = { Text(context.getString(R.string.label_port)) },
                        modifier = Modifier.weight(0.3f),
                        textStyle = TextStyle(textAlign = TextAlign.Center),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        shape = RoundedCornerShape(25)
                    )
                }

                IconButton(
                    onClick = {
                        if (isHostError || isPortError) {
                            AudioPlayer.message =
                                if (isHostError) "Check entered IP address" else "Check entered Port number"
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
                    modifier = Modifier.size(80.dp)
                        .padding(12.dp),
                ) {
                    Icon(
                        imageVector = if (started) Icons.Outlined.PauseCircleOutline else Icons.Outlined.PlayCircleOutline,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.fillMaxSize()
                    )
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
