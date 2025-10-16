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

import android.media.AudioTrack
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight.Companion.W600
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import io.github.mkckr0.audio_share_app.R
import io.github.mkckr0.audio_share_app.model.AudioConfigKeys
import io.github.mkckr0.audio_share_app.model.getFloat
import io.github.mkckr0.audio_share_app.ui.base.ConfigGroup
import io.github.mkckr0.audio_share_app.ui.base.SliderConfig
import io.github.mkckr0.audio_share_app.ui.theme.AppTheme

@Composable
fun AudioScreen() {
    val context = LocalContext.current
    Column(
        modifier = Modifier.padding(16.dp)
            .fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text(
            text = "Audio options",
            modifier = Modifier
                .fillMaxWidth()
                .weight(0.2f)
                .wrapContentHeight(Alignment.CenterVertically),
            textAlign = TextAlign.Center,
            fontWeight = W600
        )
        ConfigGroup(context.getString(R.string.label_audio_track)) {
            SliderConfig(
                key = AudioConfigKeys.VOLUME,
                title = context.getString(R.string.label_volume_linear_gain),
                valueFormatter = { "%1.2f".format(it) },
                defaultValue = context.getFloat(R.string.default_volume),
                valueRange = AudioTrack.getMinVolume()..AudioTrack.getMaxVolume(),
                step = 0.01f,
            )
            SliderConfig(
                key = AudioConfigKeys.BUFFER_SCALE,
                title = context.getString(R.string.label_buffer_scale),
                valueFormatter = { "%1.0fx".format(it) },
                defaultValue = context.getFloat(R.string.default_buffer_scale),
                valueRange = 1f..10f,
                step = 1f
            )
        }
        ConfigGroup(context.getString(R.string.label_audio_effect)) {
            SliderConfig(
                key = AudioConfigKeys.LOUDNESS_ENHANCER,
                title = context.getString(R.string.label_loudness_enhancer),
                valueFormatter = { "%1.0fmB".format(it) },
                defaultValue = context.getFloat(R.string.default_loudness_enhancer),
                valueRange = 0f..3000f,
                step = 100f
            )
        }
    }
}

@Preview
@Composable
fun AudioScreenPreview() {
    AppTheme {
        AudioScreen()
    }
}