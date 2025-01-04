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

package io.github.mkckr0.audio_share_app.worker

import android.Manifest
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.util.Log
import android.widget.Toast
import androidx.core.app.ActivityCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.contentValuesOf
import androidx.work.Worker
import androidx.work.WorkerParameters
import io.github.mkckr0.audio_share_app.BuildConfig
import io.github.mkckr0.audio_share_app.ui.MainActivity
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.Channel
import io.github.mkckr0.audio_share_app.model.LatestRelease
import io.github.mkckr0.audio_share_app.model.Notification
import io.github.mkckr0.audio_share_app.model.Util
import io.ktor.client.HttpClient
import io.ktor.client.call.body
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.client.request.get
import io.ktor.serialization.kotlinx.json.json
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonNamingStrategy

class UpdateWorker(appContext: Context, workerParams: WorkerParameters) : Worker(appContext, workerParams) {
    private val tag = javaClass.simpleName
    
    @OptIn(ExperimentalSerializationApi::class)
    override fun doWork(): Result {
        Log.d(tag, "UpdateWorker doWork")
        suppressMessage = inputData.getBoolean("SUPPRESS_MESSAGE", false)
        MainScope().launch(Dispatchers.IO) {
            val httpClient = HttpClient {
                expectSuccess = true
                install(ContentNegotiation) {
                    json(Json {
                        ignoreUnknownKeys = true
                        prettyPrint = true
                        isLenient = true
                        namingStrategy = JsonNamingStrategy.SnakeCase
                    })
                }
            }
            val res =
                httpClient.get("https://api.github.com/repos/mkckr0/audio-share/releases/latest")
            val latestRelease: LatestRelease = res.body()

            withContext(Dispatchers.Main) {
                if (!Util.isNewerVersion(latestRelease.tagName, "v${BuildConfig.VERSION_NAME}")) {
                    showMessage(applicationContext.getString(R.string.label_no_update))
                    return@withContext
                }

                val apkAsset = latestRelease.assets.find {
                    it.name.matches(Regex("audio-share-app-[0-9.]*-release.apk"))
                }
                if (apkAsset == null) {
                    showMessage(applicationContext.getString(R.string.label_has_an_update_1))
                    return@withContext
                }

                with(NotificationManagerCompat.from(applicationContext)) {
                    if (ActivityCompat.checkSelfPermission(
                            applicationContext,
                            Manifest.permission.POST_NOTIFICATIONS
                        ) != PackageManager.PERMISSION_GRANTED
                    ) {
                        showMessage("No permission to post notification")
                        return@with
                    }

                    val intent = Intent(applicationContext, MainActivity::class.java).apply {
                        flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
                    }
                    intent.putExtra("action", "update")
                    intent.putExtra("apkAsset", apkAsset)
                    val pendingIntent = PendingIntent.getActivity(
                        applicationContext,
                        0,
                        intent,
                        PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
                    )

                    val notification = NotificationCompat.Builder(
                        applicationContext,
                        Channel.UPDATE.id
                    )
                        .setSmallIcon(R.drawable.baseline_update)
                        .setContentTitle(applicationContext.getString(R.string.label_has_an_update_2).format(latestRelease.tagName))
                        .setContentText(applicationContext.getString(R.string.label_tap_notification_1))
                        .setContentIntent(pendingIntent)
                        .setAutoCancel(true)
                        .build()

                    notify(Notification.UPDATE.id, notification)
                    showMessage(applicationContext.getString(R.string.label_tap_notification_2))
                }
            }
        }
        return Result.success()
    }

    private var suppressMessage = false

    private fun showMessage(message: String) {
        if (!suppressMessage) {
            Toast.makeText(applicationContext, message, Toast.LENGTH_SHORT).show()
        }
    }
}