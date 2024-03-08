package io.github.mkckr0.audio_share_app.model

data class Channel(
    val id: String,
    val name: String,
)

class Channels {
    companion object {
        val CHANNEL_UPDATE = Channel("CHANNEL_ID_UPDATE", "Update")
    }
}

class Notifications {
    companion object {
        const val NOTIFICATION_ID_UPDATE = 1
    }
}