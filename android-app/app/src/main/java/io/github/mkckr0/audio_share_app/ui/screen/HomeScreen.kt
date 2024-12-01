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

import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.scrollable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PauseCircle
import androidx.compose.material.icons.filled.PlayCircle
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
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
import androidx.media3.common.MediaItem
import androidx.media3.common.MediaMetadata
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import io.github.mkckr0.audio_share_app.MainActivity
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.getResourceUri
import io.github.mkckr0.audio_share_app.service.AudioPlayer
import io.github.mkckr0.audio_share_app.ui.screen.HomeScreenViewModel.UiState

@Composable
fun HomeScreen(viewModel: HomeScreenViewModel = viewModel()) {
    val context = LocalContext.current
    val activity = context as MainActivity

    when (val uiState = viewModel.uiState.collectAsStateWithLifecycle().value) {
        UiState.Loading -> {}
        is UiState.Success -> {
            var host by remember(uiState.host) { mutableStateOf(uiState.host) }
            var port by remember(uiState.port) { mutableStateOf(uiState.port.toString()) }
            var started by remember { mutableStateOf(false) }
//            var message by remember { mutableStateOf("") }
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
                    OutlinedTextField(
                        value = host,
                        onValueChange = { host = it },
                        enabled = !started,
                        label = { Text("Host") },
                        modifier = Modifier.weight(0.7f),
                    )
                    OutlinedTextField(
                        value = port,
                        onValueChange = {
                            if (it.isEmpty() || it.isDigitsOnly() && it.toInt() in 1..65535) {
                                port = it
                            }
                        },
                        enabled = !started,
                        label = { Text("Port") },
                        modifier = Modifier.weight(0.3f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    )
                }

                IconButton(
                    onClick = {
                        started = !started
                        if (started) {
                            viewModel.saveNetWorkSettings(host, port.toInt())
                            activity.postToMediaController { mediaController ->
                                mediaController.run {
                                    setMediaItem(
                                        MediaItem.fromUri("tcp://$host:$port").buildUpon()
                                            .setMediaMetadata(
                                                MediaMetadata.Builder()
                                                    .setTitle("Audio Share")
                                                    .setArtist("$host:$port")
                                                    .setArtworkUri(context.getResourceUri(R.drawable.artwork))
                                                    .build()
                                            )
                                            .build()
                                    )
                                    prepare()
                                    play()
                                }
                            }
                        } else {
                            activity.postToMediaController { mediaController ->
                                mediaController.pause()
                            }
                        }
                    },
                    modifier = Modifier.size(64.dp),
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

                activity.postToMediaController { mediaController ->
                    started = mediaController.playWhenReady
                    mediaController.addListener(listener)
                }

                onStopOrDispose {
                    activity.postToMediaController { mediaController ->
                        mediaController.removeListener(listener)
                    }
                }
            }
        }
    }
}
