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

package io.github.mkckr0.audio_share_app.ui.theme

import android.app.Application
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.AppSettingsKeys
import io.github.mkckr0.audio_share_app.model.appSettingsDataStore
import io.github.mkckr0.audio_share_app.model.getBoolean
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn

class AppThemeViewModel(application: Application) : AndroidViewModel(application) {

    sealed interface UiState {
        data object Loading : UiState
        data class Success(
            val dynamicColorFromWallpaper: Boolean,
            val dynamicColorFromSeedColor: String,
        ) : UiState
    }

    val uiState: StateFlow<UiState> = application.appSettingsDataStore.data.map {
        UiState.Success(
             dynamicColorFromWallpaper = it[booleanPreferencesKey(AppSettingsKeys.DYNAMIC_COLOR_FROM_WALLPAPER)]
                ?: application.getBoolean(R.bool.default_dynamic_color_from_wallpaper),
            dynamicColorFromSeedColor = it[stringPreferencesKey(AppSettingsKeys.DYNAMIC_COLOR_FROM_SEED_COLOR)]
                ?: application.getString(R.string.default_dynamic_color_from_seed_color),
        )
    }.stateIn(
        scope = viewModelScope,
        started = SharingStarted.Eagerly,
        initialValue = UiState.Loading,
    )
}