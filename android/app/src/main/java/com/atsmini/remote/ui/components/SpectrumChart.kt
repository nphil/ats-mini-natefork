package com.atsmini.remote.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Clear
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.atsmini.remote.data.ScanData

// ---------------------------------------------------------------------------
// Frequency / level formatting (shared across Spectrum + Waterfall).
// ---------------------------------------------------------------------------

/** Format a raw scan frequency value (FM = x10 kHz, AM/SSB = kHz) for a label. */
fun fmtFreq(raw: Int, isFM: Boolean): String =
    if (isFM) String.format("%.1f", raw / 100.0) else {
        val khz = raw
        if (khz >= 1000) String.format("%.3f", khz / 1000.0) else khz.toString()
    }

fun freqUnit(isFM: Boolean): String = if (isFM) "MHz" else "kHz"

/**
 * Interactive spectrum analyser chart. Self-contained: maintains its own peak-hold,
 * exponential-average, and touch-cursor state, draws the grid/traces/markers, and
 * (optionally) renders the Peak-hold / Average / Clear controls and a Scan button.
 *
 * Touch anywhere on the trace and drag to inspect: a bright cursor follows the finger
 * with a freq + level callout. The tuned-frequency marker (the radio's current freq)
 * is drawn as a solid accent line and tracks live status updates, so retuning on the
 * Radio tab slides the line in real time. The strongest bin is flagged with a marker.
 *
 * @param scan the most recent completed scan, or null to show the empty hint.
 * @param tunedFreq the radio's current frequency (raw units), for the live marker.
 * @param showControls render the Peak hold / Average / Clear chip row.
 * @param onScan if non-null, render a Scan button wired to this callback.
 */
@Composable
fun SpectrumChart(
    scan: ScanData?,
    tunedFreq: Int,
    isFM: Boolean,
    progress: Double,
    modifier: Modifier = Modifier,
    height: Dp = 200.dp,
    showControls: Boolean = true,
    onScan: (() -> Unit)? = null,
) {
    val view = LocalView.current
    val primary = MaterialTheme.colorScheme.primary
    val secondary = MaterialTheme.colorScheme.secondary
    val tertiary = MaterialTheme.colorScheme.tertiary
    val onSurfaceVar = MaterialTheme.colorScheme.onSurfaceVariant
    val gridColor = onSurfaceVar.copy(alpha = 0.16f)
    val axisColor = onSurfaceVar.copy(alpha = 0.75f)
    val surfaceColor = MaterialTheme.colorScheme.surface

    var peakHold by remember { mutableStateOf(true) }
    var averaging by remember { mutableStateOf(true) }
    var resetKey by remember { mutableIntStateOf(0) }

    var peak by remember { mutableStateOf<IntArray?>(null) }
    var avg by remember { mutableStateOf<FloatArray?>(null) }
    LaunchedEffect(scan, resetKey) {
        val d = scan?.rssi
        if (d == null || d.isEmpty()) return@LaunchedEffect
        val arr = d.toIntArray()
        peak = peak?.takeIf { it.size == arr.size }?.let { p -> IntArray(arr.size) { maxOf(p[it], arr[it]) } } ?: arr.copyOf()
        avg = avg?.takeIf { it.size == arr.size }?.let { a -> FloatArray(arr.size) { a[it] * 0.6f + arr[it] * 0.4f } } ?: FloatArray(arr.size) { arr[it].toFloat() }
    }

    // Touch cursor: -1 hidden. isDragging styles it brighter while the finger is down.
    var cursorBin by remember { mutableIntStateOf(-1) }
    var isDragging by remember { mutableStateOf(false) }
    // Clear the cursor whenever a fresh scan replaces the data set.
    LaunchedEffect(scan) { cursorBin = -1 }

    val data = scan?.rssi
    val sf = scan?.startFreq ?: 0
    val step = scan?.step ?: 0
    val n = data?.size ?: 0
    val span = if (n > 1) step * (n - 1) else 0
    val peakIdx = data?.indices?.maxByOrNull { data[it] } ?: -1

    Column(modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        // ── Readout strip ──────────────────────────────────────────────────────
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            if (data != null && data.isNotEmpty() && span > 0) {
                Text(
                    "${fmtFreq(sf, isFM)}–${fmtFreq(sf + span, isFM)} ${freqUnit(isFM)}",
                    fontSize = 11.sp, color = onSurfaceVar,
                )
                if (peakIdx >= 0) Text(
                    "Peak ${data[peakIdx]} @ ${fmtFreq(sf + peakIdx * step, isFM)}",
                    fontSize = 11.sp, color = secondary,
                )
            } else {
                Text("Run a scan to populate the spectrum.", fontSize = 11.sp, color = onSurfaceVar)
            }
        }
        // Cursor readout (or the live tuned freq when the cursor is idle).
        if (cursorBin in 0 until n && data != null) {
            Text(
                "◆ ${fmtFreq(sf + cursorBin * step, isFM)} ${freqUnit(isFM)} · ${data[cursorBin]} dBµV",
                fontSize = 12.sp, color = tertiary,
            )
        } else if (span > 0 && tunedFreq in sf..(sf + span)) {
            Text("Tuned ${fmtFreq(tunedFreq, isFM)} ${freqUnit(isFM)}", fontSize = 12.sp, color = primary)
        }

        // ── Chart canvas ───────────────────────────────────────────────────────
        Box(Modifier.fillMaxWidth().height(height)) {
            Canvas(
                Modifier.fillMaxWidth().height(height).pointerInput(n) {
                    if (n <= 0) return@pointerInput
                    awaitEachGesture {
                        val down = awaitFirstDown()
                        isDragging = true
                        cursorBin = (down.position.x / size.width * n).toInt().coerceIn(0, n - 1)
                        Haptics.light(view)
                        down.consume()
                        while (true) {
                            val ev = awaitPointerEvent()
                            val ch = ev.changes.firstOrNull() ?: break
                            if (!ch.pressed) break
                            cursorBin = (ch.position.x / size.width * n).toInt().coerceIn(0, n - 1)
                            ch.consume()
                        }
                        isDragging = false
                    }
                },
            ) {
                if (data == null || data.isEmpty()) return@Canvas
                val maxV = (peak?.maxOrNull() ?: data.max()).coerceAtLeast(1)
                drawGrid(gridColor, axisColor, sf, step, data.size, maxV, isFM)

                val a = avg
                val useAvg = averaging && a != null && a.size == data.size
                val trace: (Int) -> Float = if (useAvg) { i -> a!![i] } else { i -> data[i].toFloat() }
                drawTrace(trace, data.size, maxV, primary, fill = true)

                if (peakHold) peak?.takeIf { it.size == data.size }?.let { p ->
                    drawTrace({ i -> p[i].toFloat() }, p.size, maxV, secondary.copy(alpha = 0.9f), fill = false)
                }

                // Strongest-signal flag: dotted line + downward triangle at the peak.
                if (peakIdx in 0 until data.size && data.size > 1) {
                    val px = peakIdx.toFloat() / (data.size - 1) * size.width
                    drawLine(secondary.copy(alpha = 0.5f), Offset(px, 0f), Offset(px, size.height),
                        strokeWidth = 1.5f, pathEffect = PathEffect.dashPathEffect(floatArrayOf(6f, 6f)))
                    val tri = Path().apply {
                        moveTo(px - 7f, 0f); lineTo(px + 7f, 0f); lineTo(px, 12f); close()
                    }
                    drawPath(tri, secondary)
                }

                // Live tuned-frequency marker (solid accent line).
                if (span > 0 && tunedFreq in sf..(sf + span)) {
                    val x = (tunedFreq - sf).toFloat() / span * size.width
                    drawLine(primary, Offset(x, 0f), Offset(x, size.height), strokeWidth = 2.5f)
                }

                // Touch cursor: bright line + dot on the trace + freq/level callout.
                if (cursorBin in 0 until data.size) {
                    val cx = cursorBin.toFloat() / (data.size - 1) * size.width
                    val cy = size.height - (trace(cursorBin) / maxV).coerceIn(0f, 1f) * size.height
                    val cursorAlpha = if (isDragging) 1f else 0.85f
                    drawLine(tertiary.copy(alpha = cursorAlpha), Offset(cx, 0f), Offset(cx, size.height),
                        strokeWidth = if (isDragging) 3f else 2f)
                    drawCircle(tertiary, radius = if (isDragging) 6f else 4f, center = Offset(cx, cy))
                    drawCallout(cx, fmtFreq(sf + cursorBin * step, isFM), "${data[cursorBin]} dBµV",
                        tertiary, surfaceColor, axisColor)
                }
            }
            if (data == null || data.isEmpty()) {
                Text("No spectrum yet", fontSize = 12.sp, color = onSurfaceVar,
                    modifier = Modifier.align(Alignment.Center))
            }
        }

        if (progress > 0 && progress < 100)
            Text("Scanning… ${progress.toInt()}%", fontSize = 12.sp, color = onSurfaceVar)

        // ── Controls ─────────────────────────────────────────────────────────────
        if (showControls) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                ToggleChip("Peak hold", peakHold) { peakHold = !peakHold }
                ToggleChip("Average", averaging) { averaging = !averaging }
                FilterChip(
                    selected = false,
                    onClick = { peak = null; avg = null; cursorBin = -1; resetKey++ },
                    label = { Text("Clear") },
                    leadingIcon = { Icon(Icons.Filled.Clear, null, Modifier.height(16.dp)) },
                )
            }
        }
        if (onScan != null) {
            Button(onClick = onScan, modifier = Modifier.fillMaxWidth()) { Text("Scan") }
        }
    }
}

/** A FilterChip with an explicit selected state: filled + check icon when active. */
@Composable
private fun ToggleChip(label: String, selected: Boolean, onClick: () -> Unit) {
    FilterChip(
        selected = selected,
        onClick = onClick,
        label = { Text(label) },
        leadingIcon = if (selected) {
            { Icon(Icons.Filled.Check, null, Modifier.height(16.dp)) }
        } else null,
        colors = FilterChipDefaults.filterChipColors(
            selectedContainerColor = MaterialTheme.colorScheme.secondaryContainer,
            selectedLabelColor = MaterialTheme.colorScheme.onSecondaryContainer,
            selectedLeadingIconColor = MaterialTheme.colorScheme.onSecondaryContainer,
        ),
    )
}

// ---------------------------------------------------------------------------
// Canvas drawing helpers.
// ---------------------------------------------------------------------------

/** Draw the dB (vertical) + frequency (horizontal) reference grid with labels. */
private fun DrawScope.drawGrid(
    grid: Color, axis: Color, startFreq: Int, step: Int, points: Int, maxV: Int, isFM: Boolean,
) {
    val paint = android.graphics.Paint().apply {
        color = axis.toArgb(); textSize = 24f; isAntiAlias = true
    }
    val divs = 5
    for (i in 0..divs) {
        val y = size.height * i / divs
        drawLine(grid, Offset(0f, y), Offset(size.width, y))
        val level = maxV * (divs - i) / divs
        if (i in 1 until divs) drawContext.canvas.nativeCanvas.drawText("$level", 6f, y - 6f, paint)
    }
    if (step > 0 && points > 1) {
        val span = step * (points - 1)
        val fdiv = 4
        paint.textAlign = android.graphics.Paint.Align.CENTER
        for (i in 0..fdiv) {
            val x = size.width * i / fdiv
            drawLine(grid, Offset(x, 0f), Offset(x, size.height))
            val f = startFreq + span * i / fdiv
            val tx = x.coerceIn(28f, size.width - 28f)
            drawContext.canvas.nativeCanvas.drawText(fmtFreq(f, isFM), tx, size.height - 6f, paint)
        }
    }
}

/** Draw a trace given a value accessor. [fill] adds a gradient area under the line. */
private fun DrawScope.drawTrace(value: (Int) -> Float, n: Int, maxV: Int, color: Color, fill: Boolean) {
    if (n < 2) return
    val dx = size.width / (n - 1)
    val line = Path()
    for (i in 0 until n) {
        val y = size.height - (value(i) / maxV).coerceIn(0f, 1f) * size.height
        if (i == 0) line.moveTo(0f, y) else line.lineTo(i * dx, y)
    }
    if (fill) {
        val area = Path().apply {
            addPath(line)
            lineTo(size.width, size.height); lineTo(0f, size.height); close()
        }
        drawPath(area, Brush.verticalGradient(listOf(color.copy(alpha = 0.45f), color.copy(alpha = 0.04f))))
    }
    drawPath(line, color, style = Stroke(width = 2.5f, cap = StrokeCap.Round))
}

/** Draw a small rounded callout bubble near the top of the cursor line. */
private fun DrawScope.drawCallout(
    cx: Float, freq: String, level: String, accent: Color, bg: Color, textColor: Color,
) {
    val pad = 10f
    val textPaint = android.graphics.Paint().apply {
        color = textColor.toArgb(); textSize = 26f; isAntiAlias = true
    }
    val freqW = textPaint.measureText(freq)
    val levelW = textPaint.measureText(level)
    val boxW = maxOf(freqW, levelW) + pad * 2
    val boxH = 70f
    var left = cx - boxW / 2
    left = left.coerceIn(2f, size.width - boxW - 2f)
    val top = 4f
    drawContext.canvas.nativeCanvas.apply {
        val bgPaint = android.graphics.Paint().apply { color = bg.toArgb(); isAntiAlias = true }
        val borderPaint = android.graphics.Paint().apply {
            color = accent.toArgb(); isAntiAlias = true
            style = android.graphics.Paint.Style.STROKE; strokeWidth = 2f
        }
        drawRoundRect(left, top, left + boxW, top + boxH, 10f, 10f, bgPaint)
        drawRoundRect(left, top, left + boxW, top + boxH, 10f, 10f, borderPaint)
        drawText(freq, left + pad, top + 28f, textPaint)
        drawText(level, left + pad, top + 58f, textPaint)
    }
}
