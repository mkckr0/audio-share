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

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import androidx.annotation.AnyRes
import androidx.annotation.BoolRes
import androidx.annotation.IntegerRes
import androidx.annotation.StringRes

fun Context.getResourceUri(@AnyRes resId: Int) : Uri {
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