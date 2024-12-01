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

package io.github.mkckr0.audio_share_app.ui.base

import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp

private val iconSize = 32.dp
private var isFirstCategory = true

data class PreferenceIntent(
    val action: String,
    val data: String?,
    val extras: Bundle?
)

@SuppressLint("ComposableNaming")
@Composable
fun rememberIntent(action: String, data: String? = null, extras: Bundle? = null): PreferenceIntent {
    return remember { PreferenceIntent(action, data, extras) }
}

@Composable
fun PreferenceScreen(content: @Composable () -> Unit) {
    val scrollState = rememberScrollState()
    Column(
        modifier = Modifier.verticalScroll(scrollState)
    ) {
        isFirstCategory = true
        content()
    }
}

@Composable
fun PreferenceCategory(title: String, content: @Composable () -> Unit) {
    Column {
        if (isFirstCategory) {
            isFirstCategory = false
        } else {
            HorizontalDivider()
        }
        Row(
            modifier = Modifier
                .background(MaterialTheme.colorScheme.surface)
                .fillMaxWidth()
                .height(48.dp)
                .padding(top = 16.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = title,
                color = MaterialTheme.colorScheme.primary,
                style = MaterialTheme.typography.titleSmall,
                modifier = Modifier.padding(start = 16.dp + iconSize + 16.dp)
            )
        }
        content()
    }
}

@Composable
fun <IconSrc> PreferenceIcon(iconSrc: IconSrc) {
    val iconModifier = Modifier.size(iconSize)
    when (iconSrc) {
        is ImageVector -> Icon(
            iconSrc,
            null,
            modifier = iconModifier,
            tint = MaterialTheme.colorScheme.primary
        )

        is Int -> Icon(
            painterResource(iconSrc),
            null,
            modifier = iconModifier,
            tint = MaterialTheme.colorScheme.primary
        )

        else -> Spacer(modifier = iconModifier)
    }
}

@Composable
private fun <IconSrc> BasePreference(
    iconSrc: IconSrc,
    title: String,
    summary: String?,
    intent: PreferenceIntent?,
    onClick: (() -> Unit)?
) {
    val context = LocalContext.current
    ListItem(
        leadingContent = { PreferenceIcon(iconSrc) },
        headlineContent = { Text(title) },
        supportingContent = summary?.let { { Text(it) } },
        modifier = Modifier.clickable(onClick != null || intent != null) {
            onClick?.invoke()
            intent?.let {
                context.startActivity(Intent(intent.action).apply {
                    if (intent.data != null) {
                        data = Uri.parse(intent.data)
                    }
                    if (intent.extras != null) {
                        putExtras(intent.extras)
                    }
                })
            }
        }
    )
}

@Composable
fun Preference(
    icon: ImageVector? = null,
    title: String,
    summary: String? = null,
    intent: PreferenceIntent? = null,
    onClick: (() -> Unit)? = null
) {
    BasePreference(icon, title, summary, intent, onClick)
}

@Composable
fun Preference(
    icon: Int,
    title: String,
    summary: String? = null,
    intent: PreferenceIntent? = null,
    onClick: (() -> Unit)? = null,
) {
    BasePreference(icon, title, summary, intent, onClick)
}
