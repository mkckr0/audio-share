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
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ListItem
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import io.github.mkckr0.audio_share_app.model.appSettingsDataStore
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch

object EditTextPreferenceDefaults {
    val onSubmit: (newValue: String) -> Boolean = { true }
}

@Composable
private fun <IconSrc> BaseEditTextPreference(
    iconSrc: IconSrc,
    key: String,
    title: String,
    defaultValue: String,
    onSubmit: (newText: String) -> Boolean
) {
    val context = LocalContext.current

    val scope = rememberCoroutineScope()
    val prefKey = remember { stringPreferencesKey(key) }
    val summary = remember {
        context.appSettingsDataStore.data.map {
            it[prefKey] ?: defaultValue
        }
    }.collectAsState(null).value

    var showDialog by remember { mutableStateOf(false) }

    ListItem(
        leadingContent = { PreferenceIcon(iconSrc) },
        headlineContent = { Text(title) },
        supportingContent = { Text(summary ?: "") },
        modifier = Modifier.clickable {
            if (summary == null) {
                return@clickable
            }
            showDialog = true
        }
    )

    if (showDialog && summary != null) {
        var textField by remember(summary) { mutableStateOf(summary) }
        AlertDialog(
            onDismissRequest = { showDialog = false },
            icon = { PreferenceIcon(iconSrc) },
            title = { Text(title) },
            text = {
                TextField(
                    value = textField,
                    onValueChange = { textField = it },
                )
            },
            dismissButton = {
                TextButton(
                    onClick = { showDialog = false }
                ) {
                    Text("Cancel")
                }
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        showDialog = false
                        textField = textField.trim()
                        if (onSubmit(textField)) {
                            scope.launch {
                                context.appSettingsDataStore.edit {
                                    it[prefKey] = textField
                                }
                            }
                        }
                    }
                ) {
                    Text("Apply")
                }
            },
        )
    }
}

@Composable
fun EditTextPreference(
    icon: ImageVector? = null,
    key: String,
    title: String,
    defaultValue: String = "",
    onSubmit: (newValue: String) -> Boolean = EditTextPreferenceDefaults.onSubmit,
) {
    BaseEditTextPreference(icon, key, title, defaultValue, onSubmit)
}

@Composable
fun EditTextPreference(
    icon: Int,
    key: String,
    title: String,
    defaultValue: String = "",
    onSubmit: (newValue: String) -> Boolean = EditTextPreferenceDefaults.onSubmit,
) {
    BaseEditTextPreference(icon, key, title, defaultValue, onSubmit)
}
