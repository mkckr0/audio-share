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
import androidx.work.Worker
import androidx.work.WorkerParameters
import com.google.android.material.snackbar.Snackbar
import io.github.mkckr0.audio_share_app.BuildConfig
import io.github.mkckr0.audio_share_app.Channel
import io.github.mkckr0.audio_share_app.MainActivity
import io.github.mkckr0.audio_share_app.Notification
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.Util
import io.github.mkckr0.audio_share_app.model.LatestRelease
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
        Log.d(tag, "doWork")
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
                if (!Util.isNewerVersion(latestRelease.name, "v${BuildConfig.VERSION_NAME}")) {
                    showMessage("No update")
                    return@withContext
                }

                val apkAsset = latestRelease.assets.find {
                    it.name.matches(Regex("audio-share-app-[0-9.]*-release.apk"))
                }
                if (apkAsset == null) {
                    showMessage("Has an update, but no APK")
                    return@withContext
                }

                with(NotificationManagerCompat.from(applicationContext)) {
                    if (ActivityCompat.checkSelfPermission(
                            applicationContext,
                            Manifest.permission.POST_NOTIFICATIONS
                        ) != PackageManager.PERMISSION_GRANTED
                    ) {
                        showMessage("Post notification permission is denied")
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
                        .setContentTitle("Has an update(${latestRelease.tagName})")
                        .setContentText("Tap this notification to start downloading")
                        .setContentIntent(pendingIntent)
                        .setAutoCancel(true)
                        .build()

                    notify(Notification.UPDATE.id, notification)
                    showMessage("Has an update, tap the notification to start downloading")
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