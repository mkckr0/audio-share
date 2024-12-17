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
import android.os.Bundle
import android.service.quicksettings.TileService
import android.util.Log
import androidx.annotation.OptIn
import androidx.media3.common.Player
import androidx.media3.common.util.UnstableApi
import androidx.media3.session.CommandButton
import androidx.media3.session.CommandButton.ICON_STOP
import androidx.media3.session.CommandButton.SLOT_BACK
import androidx.media3.session.CommandButton.SLOT_BACK_SECONDARY
import androidx.media3.session.CommandButton.SLOT_FORWARD
import androidx.media3.session.CommandButton.SLOT_FORWARD_SECONDARY
import androidx.media3.session.MediaSession
import androidx.media3.session.MediaSession.ConnectionResult.AcceptedResultBuilder
import androidx.media3.session.MediaSession.ConnectionResult.DEFAULT_SESSION_COMMANDS
import androidx.media3.session.MediaSessionService
import androidx.media3.session.SessionCommand
import androidx.media3.session.SessionResult
import com.google.common.util.concurrent.Futures
import com.google.common.util.concurrent.ListenableFuture
import io.github.mkckr0.audio_share_app.ui.MainActivity

class PlaybackService : MediaSessionService() {

    private val tag = PlaybackService::class.simpleName

    private var mediaSession: MediaSession? = null

    companion object {
        const val ACTION_STOP_SERVICE = "action_stop_service"
    }

    private val customCommandStop = SessionCommand(ACTION_STOP_SERVICE, Bundle.EMPTY)

    @OptIn(UnstableApi::class)
    override fun onCreate() {
        Log.d(tag, "onCreate")
        super.onCreate()
        mediaSession = MediaSession.Builder(this, AudioPlayer(this))
            .setSessionActivity(
                PendingIntent.getActivity(
                    applicationContext,
                    0,
                    Intent(
                        applicationContext,
                        MainActivity::class.java
                    ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
                )
            )
            .setMediaButtonPreferences(
                listOf(
                    CommandButton.Builder(ICON_STOP)
                        .setSessionCommand(customCommandStop)
                        .setSlots(
                            SLOT_BACK,
                            SLOT_FORWARD,
                            SLOT_BACK_SECONDARY,
                            SLOT_FORWARD_SECONDARY
                        )
                        .setDisplayName("Stop")
                        .setEnabled(true)
                        .build()
                )
            )
            .setCallback(object : MediaSession.Callback {

                private val tag = "MediaSession.Callback"

                override fun onConnect(
                    session: MediaSession,
                    controller: MediaSession.ControllerInfo
                ): MediaSession.ConnectionResult {
                    Log.d(tag, "onConnect ${controller.packageName} ${controller.connectionHints}")
                    return AcceptedResultBuilder(session)
                        .setAvailableSessionCommands(
                            DEFAULT_SESSION_COMMANDS.buildUpon()
                                .add(customCommandStop).build()
                        )
                        .build()
                }

                override fun onCustomCommand(
                    session: MediaSession,
                    controller: MediaSession.ControllerInfo,
                    customCommand: SessionCommand,
                    args: Bundle
                ): ListenableFuture<SessionResult> {
                    if (customCommand.customAction == ACTION_STOP_SERVICE) {
                        Log.d(tag, "ACTION_STOP_SERVICE")
                        session.player.stop()
                        stopSelf()
                        return Futures.immediateFuture(SessionResult(SessionResult.RESULT_SUCCESS))
                    }
                    return super.onCustomCommand(session, controller, customCommand, args)
                }

                override fun onDisconnected(
                    session: MediaSession,
                    controller: MediaSession.ControllerInfo
                ) {
                    Log.d(tag, "onDisconnected")
                    super.onDisconnected(session, controller)
                }

                override fun onPlaybackResumption(
                    mediaSession: MediaSession,
                    controller: MediaSession.ControllerInfo
                ): ListenableFuture<MediaSession.MediaItemsWithStartPosition> {
                    Log.d(tag, "onPlaybackResumption")
                    return super.onPlaybackResumption(mediaSession, controller)
                }
            })
            .build()
        mediaSession!!.player.addListener(object : Player.Listener {
            override fun onPlayWhenReadyChanged(playWhenReady: Boolean, reason: Int) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    TileService.requestListeningState(this@PlaybackService, ComponentName(this@PlaybackService, QsTileService::class.java))
                }
            }
        })
    }

    override fun onDestroy() {
        Log.d(tag, "onDestroy")
        mediaSession?.run {
            player.release()
            release()
            mediaSession = null
        }
        super.onDestroy()
    }

    override fun onGetSession(controllerInfo: MediaSession.ControllerInfo): MediaSession? {
        return mediaSession
    }

    override fun onTaskRemoved(rootIntent: Intent?) {
        Log.d(tag, "onTaskRemoved")
        super.onTaskRemoved(rootIntent)
    }
}