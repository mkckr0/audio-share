package io.github.mkckr0.audio_share_app.model

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable

@Serializable
@Parcelize
data class Asset(
    val name: String,
    val browserDownloadUrl: String,
    val size: Long,
    val contentType: String,
) : Parcelable

@Serializable
@Parcelize
data class LatestRelease(
    val name: String,
    val tagName: String,
    val assets: List<Asset>,
) : Parcelable