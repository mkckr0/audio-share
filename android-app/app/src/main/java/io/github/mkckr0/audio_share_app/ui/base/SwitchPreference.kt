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

import androidx.compose.foundation.clickable
import androidx.compose.material3.ListItem
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import io.github.mkckr0.audio_share_app.model.appSettingsDataStore
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch

@Composable
private fun <IconSrc> BaseSwitchPreference(
    iconSrc: IconSrc,
    key: String,
    title: String,
    defaultValue: Boolean,
    summaryOn: String,
    summaryOff: String,
    onChange: ((checked: Boolean) -> Unit)?
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val prefKey = remember { booleanPreferencesKey(key) }

    val checked = remember {
        context.appSettingsDataStore.data.map {
            it[prefKey] ?: defaultValue
        }
    }.collectAsState(null).value  // null states loading

    ListItem(
        leadingContent = { PreferenceIcon(iconSrc) },
        headlineContent = { Text(title) },
        supportingContent = if (checked != null && (summaryOn.isNotEmpty() || summaryOff.isNotEmpty())) {
            { Text(if (checked) summaryOn else summaryOff) }
        } else {
            null
        },
        trailingContent = {
            if (checked != null) {
                Switch(
                    checked = checked,
                    onCheckedChange = null,
                )
            }
        },
        modifier = Modifier.clickable(enabled = checked != null) {
            if (checked != null) {
                scope.launch {
                    context.appSettingsDataStore.edit {
                        it[prefKey] = !checked
                    }
                }
                onChange?.invoke(checked)
            }
        }
    )
}

@Composable
fun SwitchPreference(
    icon: ImageVector? = null,
    key: String,
    title: String,
    defaultValue: Boolean = false,
    summaryOn: String = "",
    summaryOff: String = "",
    onChange: ((checked: Boolean) -> Unit)? = null,
) {
    BaseSwitchPreference(icon, key, title, defaultValue, summaryOn, summaryOff, onChange)
}

@Composable
fun SwitchPreference(
    icon: Int? = null,
    key: String,
    title: String,
    defaultValue: Boolean = false,
    summaryOn: String = "",
    summaryOff: String = "",
    onChange: ((checked: Boolean) -> Unit)? = null,
) {
    BaseSwitchPreference(icon, key, title, defaultValue, summaryOn, summaryOff, onChange)
}