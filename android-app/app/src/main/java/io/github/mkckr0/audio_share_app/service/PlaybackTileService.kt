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

import android.os.Build
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import android.util.Log
import androidx.annotation.RequiresApi
import io.github.mkckr0.audio_share_app.AudioShareApp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

@RequiresApi(Build.VERSION_CODES.N)
class PlaybackTileService : TileService() {

    private val tag = PlaybackTileService::class.simpleName

    private val scope = MainScope()
    private var mutex = Mutex()

    override fun onClick() {
        super.onClick()
        Log.d(tag, "onClick")
        launchWithLock {
            val app = application as AudioShareApp
            app.getMediaController().run {
                if (qsTile.state == Tile.STATE_ACTIVE) {
                    pause()
                    toggleState(false)
                } else {
                    setMediaItem(app.getMediaItem())
                    prepare()
                    play()
                    toggleState(true)
                }
            }
        }
    }

    override fun onStartListening() {
        super.onStartListening()
        Log.d(tag, "onStartListening")
        launchWithLock {
            (application as AudioShareApp).getMediaController().run {
                toggleState(playWhenReady) // initial state
            }
        }
    }

    private fun toggleState(isRunning: Boolean) {
        Log.d(tag, "toggleState: $isRunning")
        qsTile.apply {
            state = if (isRunning) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                subtitle = if (state == Tile.STATE_ACTIVE) "On" else "Off"
            }
        }.updateTile()
    }

    private fun launchWithLock(block: suspend CoroutineScope.() -> Unit) {
        scope.launch {
            mutex.withLock {
                block()
            }
        }
    }
}