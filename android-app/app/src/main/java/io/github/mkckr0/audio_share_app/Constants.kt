package io.github.mkckr0.audio_share_app

import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
import android.os.Build

enum class WorkName(val value: String)
{
    AUTO_CHECK_UPDATE("auto_check_update"),
    CHECK_UPDATE("check_update"),
}

enum class Channel(val id: String, val title: String, val importance: Int) {
    @SuppressLint("InlinedApi")
    UPDATE("CHANNEL_ID_UPDATE", "Update", NotificationManager.IMPORTANCE_DEFAULT),
}

enum class Notification(val id: Int) {
    UPDATE(1)
}