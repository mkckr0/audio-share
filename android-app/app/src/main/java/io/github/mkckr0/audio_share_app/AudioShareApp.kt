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

package io.github.mkckr0.audio_share_app

import android.app.Application
import android.content.ComponentName
import android.os.Build
import android.service.quicksettings.TileService
import android.util.Log
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.media3.common.MediaItem
import androidx.media3.common.MediaMetadata
import androidx.media3.common.Player
import androidx.media3.session.MediaController
import androidx.media3.session.SessionToken
import com.google.common.util.concurrent.ListenableFuture
import io.github.mkckr0.audio_share_app.model.NetworkConfigKeys
import io.github.mkckr0.audio_share_app.model.networkConfigDataStore
import io.github.mkckr0.audio_share_app.service.PlaybackService
import io.github.mkckr0.audio_share_app.service.PlaybackTileService
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.guava.await
import kotlinx.coroutines.launch

class AudioShareApp : Application() {

    private lateinit var _mediaControllerFuture: ListenableFuture<MediaController>

    override fun onCreate() {
        super.onCreate()

        val sessionToken =
            SessionToken(this, ComponentName(this, PlaybackService::class.java))
        _mediaControllerFuture = MediaController.Builder(this, sessionToken).buildAsync()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            MainScope().launch {
                getMediaController().addListener(object : Player.Listener {
                    override fun onPlayWhenReadyChanged(playWhenReady: Boolean, reason: Int) {
                        TileService.requestListeningState(
                            applicationContext,
                            ComponentName(applicationContext, PlaybackTileService::class.java)
                        )
                        Log.d(PlaybackTileService::class.simpleName, "onPlayWhenReadyChanged")
                    }
                })
            }
        }
    }

    suspend fun getMediaController(): MediaController {
        return _mediaControllerFuture.await()
    }

    suspend fun getMediaItem(): MediaItem {
        val networkConfig = networkConfigDataStore.data.first()
        val host = networkConfig[stringPreferencesKey(NetworkConfigKeys.HOST)]
            ?: getString(R.string.default_host)
        val port = networkConfig[intPreferencesKey(NetworkConfigKeys.PORT)]
            ?: getInteger(R.integer.default_port)
        return MediaItem.fromUri("tcp://$host:$port").buildUpon()
            .setMediaMetadata(
                MediaMetadata.Builder()
                    .setTitle("Audio Share")
                    .setArtist("$host:$port")
                    .setArtworkUri(getResourceUri(R.drawable.artwork))
                    .build()
            )
            .build()
    }
}
