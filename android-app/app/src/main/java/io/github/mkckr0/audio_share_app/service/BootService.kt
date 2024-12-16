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

import android.app.Service
import android.content.ComponentName
import android.content.Intent
import android.os.IBinder
import android.util.Log
import androidx.core.os.bundleOf
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.media3.session.MediaController
import androidx.media3.session.SessionToken
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.AppSettingsKeys
import io.github.mkckr0.audio_share_app.model.appSettingsDataStore
import io.github.mkckr0.audio_share_app.model.getBoolean
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.guava.await
import kotlinx.coroutines.launch
import kotlin.time.Duration.Companion.seconds

class BootService : Service() {

    private val tag = BootService::class.simpleName

    override fun onCreate() {
        super.onCreate()
        Log.d(tag, "onCreate")
    }

    override fun onDestroy() {
        Log.d(tag, "onDestroy")
        super.onDestroy()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(tag, "onStartCommand")
        MainScope().launch {
            val appSettings = appSettingsDataStore.data.first()
            val autoStart = appSettings[booleanPreferencesKey(AppSettingsKeys.START_PLAYBACK_WHEN_SYSTEM_BOOT)] ?: getBoolean(R.bool.default_start_playback_when_system_boot)

            if (autoStart) {
                val sessionToken =
                    SessionToken(this@BootService, ComponentName(this@BootService, PlaybackService::class.java))
                val mediaController = MediaController.Builder(this@BootService, sessionToken)
                    .setConnectionHints(bundleOf("src" to "BootService"))
                    .buildAsync().await()
                mediaController.play()
                delay(3.seconds)
                mediaController.release()
            }

            stopSelf()
        }
        return START_NOT_STICKY
    }

    override fun onBind(p0: Intent?): IBinder? {
        return null
    }
}