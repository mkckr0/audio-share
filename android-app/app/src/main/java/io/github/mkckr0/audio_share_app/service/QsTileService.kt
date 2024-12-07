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

import android.app.PendingIntent
import android.content.ComponentName
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import android.util.Log
import androidx.annotation.OptIn
import androidx.annotation.RequiresApi
import androidx.media3.common.util.UnstableApi
import androidx.media3.session.MediaController
import androidx.media3.session.SessionError
import androidx.media3.session.SessionToken
import io.github.mkckr0.audio_share_app.model.canStartForegroundService
import io.github.mkckr0.audio_share_app.ui.MainActivity
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.guava.await
import kotlinx.coroutines.launch

@RequiresApi(Build.VERSION_CODES.N)
class QsTileService : TileService() {

    private val tag = QsTileService::class.simpleName

    private val scope = MainScope()
    private var _mediaController: MediaController? = null

    override fun onBind(intent: Intent?): IBinder? {
        Log.d(tag, "onBind")
        return super.onBind(intent)
    }

    override fun onUnbind(intent: Intent?): Boolean {
        Log.d(tag, "onUnbind")
        return super.onUnbind(intent)
    }

    override fun onStartListening() {
        Log.d(tag, "onStartListening")
        super.onStartListening()
        scope.launch {
            getMediaController().run {
                toggleState(playWhenReady)
            }
        }
    }

    override fun onStopListening() {
        Log.d(tag, "onStopListening")
        super.onStopListening()
        _mediaController?.release()
    }

    override fun onClick() {
        Log.d(tag, "onClick")
        super.onClick()
        scope.launch {
            if (qsTile.state == Tile.STATE_ACTIVE) {
                getMediaController().stop()
                toggleState(false)
            } else {
                if (applicationContext.canStartForegroundService()) {
                    getMediaController().play()
                    toggleState(true)
                } else {
                    Log.d(tag, "can't start foreground service")
                    val intent = Intent(
                        applicationContext,
                        MainActivity::class.java
                    ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                        val pendingIntent = PendingIntent.getActivity(
                            applicationContext,
                            0,
                            intent,
                            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
                        )
                        startActivityAndCollapse(pendingIntent)
                    } else {
                        @Suppress("DEPRECATION", "StartActivityAndCollapseDeprecated")
                        startActivityAndCollapse(intent)
                    }
                }
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

    @OptIn(UnstableApi::class)
    private suspend fun getMediaController(): MediaController {
        if (_mediaController == null) {
            val sessionToken =
                SessionToken(this, ComponentName(this, PlaybackService::class.java))
            _mediaController = MediaController.Builder(this, sessionToken)
                .setListener(object : MediaController.Listener {
                    override fun onDisconnected(controller: MediaController) {
                        super.onDisconnected(controller)
                        Log.d(tag, "onDisconnected")
                        _mediaController = null
                    }

                    override fun onError(controller: MediaController, sessionError: SessionError) {
                        super.onError(controller, sessionError)
                        Log.d(tag, "onError $sessionError")
                    }
                })
                .buildAsync().await()
        }
        return _mediaController!!
    }
}