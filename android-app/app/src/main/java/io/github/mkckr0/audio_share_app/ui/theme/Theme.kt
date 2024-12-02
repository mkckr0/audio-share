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

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.materialkolor.dynamicColorScheme

import io.github.mkckr0.audio_share_app.ui.theme.AppThemeViewModel.UiState

fun Color.Companion.parseColor(colorString: String): Color {
    return Color(android.graphics.Color.parseColor(colorString))
}

fun isDynamicColorFromWallpaperAvailable(): Boolean {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
}

@Composable
fun AppTheme(
    viewModel: AppThemeViewModel = viewModel(),
    content: @Composable () -> Unit
) {
    val context = LocalContext.current
    val isDarkTheme = isSystemInDarkTheme()

    when (val uiState = viewModel.uiState.collectAsStateWithLifecycle().value) {
        UiState.Loading -> {}
        is UiState.Success -> {
            val colorScheme = remember(
                isDarkTheme,
                uiState.dynamicColorFromWallpaper,
                uiState.dynamicColorFromSeedColor,
            ) {
                if (uiState.dynamicColorFromWallpaper && isDynamicColorFromWallpaperAvailable()) {
                    if (isDarkTheme) {
                        dynamicDarkColorScheme(context)
                    } else {
                        dynamicLightColorScheme(context)
                    }
                } else {
                    dynamicColorScheme(
                        Color.parseColor(uiState.dynamicColorFromSeedColor),
                        isDarkTheme,
                        false
                    )
                }
            }
            MaterialTheme(
                colorScheme = colorScheme,
                typography = AppTypography,
                content = content
            )
        }
    }
}
