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

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.background
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsDraggedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathOperation
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.graphics.drawscope.scale
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import io.github.mkckr0.audio_share_app.model.audioConfigDataStore
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

@Composable
fun ConfigGroup(title: String, content: @Composable () -> Unit) {
    OutlinedCard(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleMedium
            )

            Spacer(Modifier.height(8.dp))

            content()
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SliderConfig(
    key: String,
    title: String,
    valueFormatter: (value: Float) -> String,
    defaultValue: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    step: Float,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    val value = remember {
        context.audioConfigDataStore.data.map {
            it[floatPreferencesKey(key)] ?: defaultValue
        }
    }.collectAsState(null).value

    if (value != null) {
        var tempValue by remember(value) { mutableFloatStateOf(value) }
        Column(
            modifier = Modifier.padding(top = 12.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(title, style = MaterialTheme.typography.bodyMedium)
                //Text(valueFormatter(tempValue), style = MaterialTheme.typography.bodyMedium)
            }

            LineSlider(
                value = tempValue,
                onValueChange = { tempValue = it },
                onValueChangeFinished = {
                    scope.launch {
                        context.audioConfigDataStore.edit {
                            it[floatPreferencesKey(key)] = tempValue
                        }
                    }
                },
                steps = ((valueRange.endInclusive - valueRange.start) / step).toInt() - 1,
                valueRange = valueRange,
                thumbDisplay = valueFormatter,
                modifier = Modifier.padding(top = 4.dp, start = 2.dp, end = 2.dp),
            )
        }
    }
}


/**
 * A custom slider with a line track that curves up when dragged.
 * It is highly configurable and can be used with any range and number of steps.
 * Inspired, admired and got from https://www.sinasamaki.com/custom-material-3-sliders-in-jetpack-compose/
 * Made it fit into existing method
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun LineSlider(
    value: Float,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    onValueChangeFinished: () -> Unit = {},
    valueRange: ClosedFloatingPointRange<Float> = 0f..1f,
    steps: Int = 0,
    thumbDisplay: (Float) -> String = { "" },
) {
    val animatedValue by animateFloatAsState(
        targetValue = value,
        animationSpec = spring(dampingRatio = Spring.DampingRatioLowBouncy),
        label = "animatedValue"
    )
    val interaction = remember { MutableInteractionSource() }
    val isDragging by interaction.collectIsDraggedAsState()
    val density = LocalDensity.current
    val offsetHeight by animateFloatAsState(
        targetValue = with(density) { if (isDragging) 36.dp.toPx() else 0.dp.toPx() },
        animationSpec = spring(
            stiffness = Spring.StiffnessMediumLow,
            dampingRatio = Spring.DampingRatioLowBouncy
        ),
        label = "offsetAnimation"
    )
    val thumbSize = if(animatedValue>1000) 68.dp
    else if(animatedValue > 100) 60.dp
    else 48.dp

    Slider(
        value = animatedValue,
        onValueChange = onValueChange,
        modifier = modifier,
        valueRange = valueRange,
        steps = steps,
        onValueChangeFinished = onValueChangeFinished,
        interactionSource = interaction,
        thumb = {},
        track = {
            val fraction by remember {
                derivedStateOf {
                    (animatedValue - valueRange.start) / (valueRange.endInclusive - valueRange.start)
                }
            }
            var width by remember { mutableIntStateOf(0) }

            Box(
                Modifier
                    .clearAndSetSemantics { }
                    .height(thumbSize)
                    .fillMaxWidth()
                    .onSizeChanged { width = it.width },
            ) {
                val strokeColor = MaterialTheme.colorScheme.onSurface
                val isLtr = LocalLayoutDirection.current == LayoutDirection.Ltr
                Box(
                    Modifier
                        .align(Alignment.Center)
                        .fillMaxWidth()
                        .drawWithCache {
                            onDrawBehind {
                                scale(scaleY = 1f, scaleX = if (isLtr) 1f else -1f) {
                                    drawSliderPath(
                                        fraction = fraction,
                                        offsetHeight = offsetHeight,
                                        color = strokeColor,
                                    )
                                }
                            }
                        })
                Box(
                    Modifier
                        .align(Alignment.CenterStart)
                        .offset {
                            val thumbPx = with(density) { thumbSize.toPx() }
                            IntOffset(
                                x = linearInterpolation(
                                    start = -(thumbPx / 2),
                                    end = width - (thumbPx / 2),
                                    t = fraction
                                ).roundToInt(),
                                y = -offsetHeight.roundToInt(),
                            )
                        }
                        .size(thumbSize)
                        .padding(10.dp)
                        .shadow(elevation = 4.dp, shape = CircleShape)
                        .background(color = MaterialTheme.colorScheme.primary, shape = CircleShape),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(
                        thumbDisplay(animatedValue),
                        style = MaterialTheme.typography.labelSmall,
                        color = Color.Black
                    )
                }
            }
        }
    )
}

private fun linearInterpolation(start: Float, end: Float, t: Float): Float {
    return start + (end - start) * t
}

private fun DrawScope.drawSliderPath(
    fraction: Float,
    offsetHeight: Float,
    color: Color,
) {
    val path = Path()
    val activeWidth = size.width * fraction
    val midPointHeight = size.height / 2
    val curveHeight = midPointHeight - offsetHeight
    val beyondBounds = size.width * 2
    val ramp = 60.dp.toPx()

    path.moveTo(x = beyondBounds, y = midPointHeight)
    path.lineTo(x = activeWidth + ramp, y = midPointHeight)
    path.cubicTo(
        x1 = activeWidth + (ramp / 2), y1 = midPointHeight,
        x2 = activeWidth + (ramp / 2), y2 = curveHeight,
        x3 = activeWidth, y3 = curveHeight
    )
    path.cubicTo(
        x1 = activeWidth - (ramp / 2), y1 = curveHeight,
        x2 = activeWidth - (ramp / 2), y2 = midPointHeight,
        x3 = activeWidth - ramp, y3 = midPointHeight
    )
    path.lineTo(x = -beyondBounds, y = midPointHeight)

    val variation = .1f
    path.lineTo(x = -beyondBounds, y = midPointHeight + variation)
    path.lineTo(x = activeWidth - ramp, y = midPointHeight + variation)
    path.cubicTo(
        x1 = activeWidth - (ramp / 2), y1 = midPointHeight + variation,
        x2 = activeWidth - (ramp / 2), y2 = curveHeight + variation,
        x3 = activeWidth, y3 = curveHeight + variation
    )
    path.cubicTo(
        x1 = activeWidth + (ramp / 2), y1 = curveHeight + variation,
        x2 = activeWidth + (ramp / 2), y2 = midPointHeight + variation,
        x3 = activeWidth + ramp, y3 = midPointHeight + variation
    )
    path.lineTo(x = beyondBounds, y = midPointHeight + variation)

    val exclude = Path().apply {
        addRect(Rect(-beyondBounds, -beyondBounds, 0f, beyondBounds))
        addRect(Rect(size.width, -beyondBounds, beyondBounds, beyondBounds))
    }
    val trimmedPath = Path()
    trimmedPath.op(path, exclude, PathOperation.Difference)

    clipRect(
        left = -beyondBounds,
        top = -beyondBounds,
        bottom = beyondBounds,
        right = activeWidth,
    ) {
        drawTrimmedPath(trimmedPath, color)
    }
    clipRect(
        left = activeWidth,
        top = -beyondBounds,
        bottom = beyondBounds,
        right = beyondBounds,
    ) {
        drawTrimmedPath(trimmedPath, color.copy(alpha = .2f))
    }
}

private fun DrawScope.drawTrimmedPath(path: Path, color: Color) {
    drawPath(
        path = path,
        color = color,
        style = Stroke(width = 10f, cap = StrokeCap.Round, join = StrokeJoin.Round),
    )
}
