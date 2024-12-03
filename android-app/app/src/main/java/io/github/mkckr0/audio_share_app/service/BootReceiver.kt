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

import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.util.Log
import androidx.media3.session.MediaController
import androidx.media3.session.SessionToken
import kotlinx.coroutines.delay
import kotlinx.coroutines.guava.await
import kotlinx.coroutines.runBlocking
import kotlin.time.Duration.Companion.seconds

class BootReceiver : BroadcastReceiver() {

    private val tag = BootReceiver::class.simpleName

    override fun onReceive(context: Context, intent: Intent) {
        when(intent.action) {
            Intent.ACTION_BOOT_COMPLETED -> {
                Log.d(tag, "onReceive $intent")
                runBlocking {
                    val sessionToken =
                        SessionToken(context.applicationContext, ComponentName(context.applicationContext, PlaybackService::class.java))
                    val mediaController = MediaController.Builder(context.applicationContext, sessionToken).buildAsync().await()
                    mediaController.play()
                    delay(1.seconds)
                    mediaController.release()
                }
            }
        }
    }
}