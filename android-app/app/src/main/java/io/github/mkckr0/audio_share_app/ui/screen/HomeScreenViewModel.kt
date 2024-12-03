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

package io.github.mkckr0.audio_share_app.ui.screen

import android.app.Application
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.NetworkConfigKeys
import io.github.mkckr0.audio_share_app.model.getInteger
import io.github.mkckr0.audio_share_app.model.networkConfigDataStore
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class HomeScreenViewModel(application: Application) : AndroidViewModel(application) {

    sealed interface UiState {
        data object Loading : UiState
        data class Success(
            val host: String,
            val port: Int,
        ) : UiState
    }

    val uiState: StateFlow<UiState> = application.networkConfigDataStore.data.map {
        UiState.Success(
            host = it[stringPreferencesKey(NetworkConfigKeys.HOST)] ?: application.getString(R.string.default_host),
            port = it[intPreferencesKey(NetworkConfigKeys.PORT)] ?: application.getInteger(R.integer.default_port)
        )
    }.stateIn(
        scope = viewModelScope,
        started = SharingStarted.Eagerly,
        initialValue = UiState.Loading,
    )

    fun saveNetWorkSettings(host: String, port: Int): Job {
        return viewModelScope.launch {
            getApplication<Application>().networkConfigDataStore.edit {
                it[stringPreferencesKey(NetworkConfigKeys.HOST)] = host
                it[intPreferencesKey(NetworkConfigKeys.PORT)] = port
            }
        }
    }
}