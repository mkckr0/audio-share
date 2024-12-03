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
import android.content.Context
import android.content.Intent
import android.util.Log

class BootReceiver : BroadcastReceiver() {

    private val tag = BootReceiver::class.simpleName

    override fun onReceive(context: Context, intent: Intent) {
        Log.d(tag, "onReceive $intent")
        when(intent.action) {
            Intent.ACTION_BOOT_COMPLETED -> {
                // https://developer.android.com/about/versions/15/behavior-changes-15#fgs-boot-completed
                context.startService(Intent(context, BootService::class.java))
            }
        }
    }
}