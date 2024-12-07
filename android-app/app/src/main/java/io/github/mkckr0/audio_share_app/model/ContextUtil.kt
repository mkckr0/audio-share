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

package io.github.mkckr0.audio_share_app.model

import android.app.ActivityManager
import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.PowerManager
import androidx.annotation.AnyRes
import androidx.annotation.BoolRes
import androidx.annotation.IntegerRes
import androidx.annotation.StringRes
import io.github.mkckr0.audio_share_app.BuildConfig

fun Context.getResourceUri(@AnyRes resId: Int): Uri {
    return Uri.Builder()
        .scheme(ContentResolver.SCHEME_ANDROID_RESOURCE)
        .authority(resources.getResourcePackageName(resId))
        .path(resId.toString())
        .build()
}

fun Context.getBoolean(@BoolRes resId: Int): Boolean {
    return resources.getBoolean(resId)
}

fun Context.getInteger(@IntegerRes resId: Int): Int {
    return resources.getInteger(resId)
}

fun Context.getFloat(@StringRes resId: Int): Float {
    return resources.getString(resId).toFloat()
}

/**
 * https://developer.android.com/develop/background-work/services/foreground-services#background-start-restriction-exemptions
 */
fun Context.canStartForegroundService(): Boolean {

    val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
    if (powerManager.isIgnoringBatteryOptimizations(BuildConfig.APPLICATION_ID)) {
        return true
    }

    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
        return true
    }

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S_V2) {
        val activityManager = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        if (activityManager.appTasks.find { it.taskInfo.isVisible } != null) {
            return true
        }
    }

    return false
}