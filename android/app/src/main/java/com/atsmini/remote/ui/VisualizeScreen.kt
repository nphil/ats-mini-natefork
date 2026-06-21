package com.atsmini.remote.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AssistChipDefaults
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.atsmini.remote.data.Protocol
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.ui.components.Haptics
import com.atsmini.remote.ui.components.SectionCard

/** Available waterfall/intensity color palettes (common SDR choices). */
private enum class Palette(val label: String) { TURBO("Turbo"), VIRIDIS("Viridis"), CLASSIC("Classic"), GRAY("Gray") }

@Composable
fun VisualizeScreen(modifier: Modifier = Modifier) {
    var tab by remember { mutableIntStateOf(0) }
    var palette by remember { mutableStateOf(Palette.TURBO) }
    var peakHold by remember { mutableStateOf(true) }
    var averaging by remember { mutableStateOf(true) }

    Column(
        modifier = modifier.fillMaxWidth().padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            SegmentedButton(selected = tab == 0, onClick = { tab = 0 },
                shape = SegmentedButtonDefaults.itemShape(0, 2)) { Text("Spectrum") }
            SegmentedButton(selected = tab == 1, onClick = { tab = 1 },
                shape = SegmentedButtonDefaults.itemShape(1, 2)) { Text("Waterfall") }
        }
        if (tab == 0) SpectrumCard(peakHold, averaging, { peakHold = it }, { averaging = it })
        else WaterfallCard(palette, { palette = it })
    }
}

// ---------------------------------------------------------------------------
// Frequency / level formatting helpers (shared by both views).
// ---------------------------------------------------------------------------

/** Format a raw scan frequency value (FM = x10 kHz, AM/SSB = kHz) for an axis label. */
private fun fmtFreq(raw: Int, isFM: Boolean): String =
    if (isFM) String.format("%.1f", raw / 100.0) else {
        val khz = raw
        if (khz >= 1000) String.format("%.2f", khz / 1000.0) else khz.toString()
    }

private fun freqUnit(isFM: Boolean): String = if (isFM) "MHz" else "kHz"

// ---------------------------------------------------------------------------
// Spectrum
// ---------------------------------------------------------------------------

@Composable
private fun SpectrumCard(
    peakHold: Boolean,
    averaging: Boolean,
    onPeakHold: (Boolean) -> Unit,
    onAveraging: (Boolean) -> Unit,
) {
    val scan by RadioRepository.scan.collectAsStateWithLifecycle()
    val progress by RadioRepository.scanProgress.collectAsStateWithLifecycle()
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    val view = LocalView.current

    val primary = MaterialTheme.colorScheme.primary
    val secondary = MaterialTheme.colorScheme.secondary
    val gridColor = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.18f)
    val axisColor = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
    val markerColor = MaterialTheme.colorScheme.tertiary

    // Peak-hold and exponentially-averaged traces, recomputed only on new scan data.
    var peak by remember { mutableStateOf<IntArray?>(null) }
    var avg by remember { mutableStateOf<FloatArray?>(null) }
    var resetKey by remember { mutableIntStateOf(0) }
    LaunchedEffect(scan, resetKey) {
        val d = scan?.rssi ?: return@LaunchedEffect
        if (d.isEmpty()) return@LaunchedEffect
        val arr = d.toIntArray()
        peak = peak?.takeIf { it.size == arr.size }?.let { p -> IntArray(arr.size) { maxOf(p[it], arr[it]) } } ?: arr.copyOf()
        avg = avg?.takeIf { it.size == arr.size }?.let { a -> FloatArray(arr.size) { a[it] * 0.6f + arr[it] * 0.4f } } ?: FloatArray(arr.size) { arr[it].toFloat() }
    }
    // A tap drops a movable marker; -1 means hidden.
    var markerBin by remember { mutableIntStateOf(-1) }

    SectionCard(title = "Spectrum") {
        val data = scan?.rssi
        val sf = scan?.startFreq ?: 0
        val step = scan?.step ?: 0
        val peakIdx = data?.indices?.maxByOrNull { data[it] } ?: -1

        // Readout strip: peak, noise floor, span and (if placed) marker.
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            if (data != null && data.isNotEmpty()) {
                val unit = freqUnit(status.isFM)
                Text("Peak ${data[peakIdx]} @ ${fmtFreq(sf + peakIdx * step, status.isFM)} $unit",
                    fontSize = 11.sp, color = secondary)
                Text("Floor ${data.min()}", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                Text("Run a scan to populate the spectrum.", fontSize = 11.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        if (markerBin >= 0 && data != null && markerBin < data.size) {
            Text("Marker ${data[markerBin]} dBµV @ ${fmtFreq(sf + markerBin * step, status.isFM)} ${freqUnit(status.isFM)}",
                fontSize = 11.sp, color = markerColor)
        }

        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val h = if (maxWidth >= 600.dp) 280.dp else 200.dp
            Canvas(
                Modifier.fillMaxWidth().height(h).pointerInput(data?.size) {
                    detectTapGestures { o ->
                        val n = data?.size ?: return@detectTapGestures
                        markerBin = (o.x / size.width * n).toInt().coerceIn(0, n - 1)
                        Haptics.light(view)
                    }
                },
            ) {
                if (data == null || data.isEmpty()) return@Canvas
                val maxV = (peak?.maxOrNull() ?: data.max()).coerceAtLeast(1)
                drawGrid(gridColor, axisColor, sf, step, data.size, maxV, status.isFM)

                // Averaged or raw filled trace (fall back to raw if the EMA array
                // hasn't caught up to a new scan's point count yet).
                val a = avg
                val useAvg = averaging && a != null && a.size == data.size
                val trace: (Int) -> Float = if (useAvg) { i -> a!![i] } else { i -> data[i].toFloat() }
                drawTrace(trace, data.size, maxV, primary, fill = true)

                // Peak-hold overlay line.
                if (peakHold) peak?.takeIf { it.size == data.size }?.let { p ->
                    drawTrace({ i -> p[i].toFloat() }, p.size, maxV, secondary.copy(alpha = 0.9f), fill = false)
                }

                // Tuned-frequency marker (radio's current freq, if within the scan span).
                val span = step * (data.size - 1)
                if (span > 0 && status.frequency in sf..(sf + span)) {
                    val x = (status.frequency - sf).toFloat() / span * size.width
                    drawLine(markerColor.copy(alpha = 0.6f), Offset(x, 0f), Offset(x, size.height), strokeWidth = 2f)
                }
                // User marker.
                if (markerBin in 0 until data.size) {
                    val x = markerBin.toFloat() / (data.size - 1) * size.width
                    drawLine(markerColor, Offset(x, 0f), Offset(x, size.height), strokeWidth = 2f)
                }
            }
        }

        if (progress > 0) Text("Scanning… ${progress.toInt()}%", fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant)

        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(selected = peakHold, onClick = { onPeakHold(!peakHold) }, label = { Text("Peak hold") })
            FilterChip(selected = averaging, onClick = { onAveraging(!averaging) }, label = { Text("Average") })
            AssistChip(onClick = { peak = null; avg = null; markerBin = -1; resetKey++ },
                label = { Text("Reset") },
                colors = AssistChipDefaults.assistChipColors())
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { Haptics.medium(view); RadioRepository.send(Protocol.scan(1)) },
                modifier = Modifier.weight(1f)) { Text("Scan") }
        }
    }
}

/** Draw the dB (vertical) + frequency (horizontal) reference grid with labels. */
private fun DrawScope.drawGrid(
    grid: Color, axis: Color, startFreq: Int, step: Int, points: Int, maxV: Int, isFM: Boolean,
) {
    val paint = android.graphics.Paint().apply {
        color = axis.toArgb(); textSize = 24f; isAntiAlias = true
    }
    // Horizontal dB lines (5 divisions) with level labels.
    val divs = 5
    for (i in 0..divs) {
        val y = size.height * i / divs
        drawLine(grid, Offset(0f, y), Offset(size.width, y))
        val level = maxV * (divs - i) / divs
        if (i in 1 until divs) drawContext.canvas.nativeCanvas.drawText("$level", 6f, y - 6f, paint)
    }
    // Vertical frequency lines (4 divisions) with frequency labels along the bottom.
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

// ---------------------------------------------------------------------------
// Waterfall
// ---------------------------------------------------------------------------

@Composable
private fun WaterfallCard(palette: Palette, onPalette: (Palette) -> Unit) {
    val rows by RadioRepository.waterfall.collectAsStateWithLifecycle()
    val meta by RadioRepository.waterfallMeta.collectAsStateWithLifecycle()
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    val active = RadioRepository.isConnected
    val view = LocalView.current
    val axisColor = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)

    // Build the waterfall image off the row list; rebuilt only when rows change.
    val bitmap: ImageBitmap? = remember(rows, palette) { buildWaterfall(rows, palette) }

    SectionCard(title = "Waterfall") {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(if (rows.isEmpty()) "Start a live waterfall to watch a band over time."
                 else "${rows.size} lines • newest on top",
                fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            if (!active) Text("Not connected", fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
        }

        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val h = if (maxWidth >= 600.dp) 360.dp else 240.dp
            Canvas(Modifier.fillMaxWidth().height(h)) {
                if (bitmap == null) return@Canvas
                drawImage(
                    image = bitmap,
                    srcOffset = androidx.compose.ui.unit.IntOffset.Zero,
                    srcSize = androidx.compose.ui.unit.IntSize(bitmap.width, bitmap.height),
                    dstSize = androidx.compose.ui.unit.IntSize(size.width.toInt(), size.height.toInt()),
                )
                // Frequency axis labels across the top.
                meta?.let { m ->
                    if (m.step > 0 && m.pointCount > 1) {
                        val paint = android.graphics.Paint().apply {
                            color = axisColor.toArgb(); textSize = 24f; isAntiAlias = true
                            textAlign = android.graphics.Paint.Align.CENTER
                        }
                        val span = m.step * (m.pointCount - 1)
                        for (i in 0..4) {
                            val x = (size.width * i / 4).coerceIn(28f, size.width - 28f)
                            val f = m.startFreq + span * i / 4
                            drawContext.canvas.nativeCanvas.drawText(fmtFreq(f, status.isFM), x, 24f, paint)
                        }
                    }
                }
            }
        }

        // Intensity legend.
        Canvas(Modifier.fillMaxWidth().height(10.dp)) {
            val n = 64
            val w = size.width / n
            for (i in 0 until n) {
                drawRect(Color(paletteArgb((i * 255 / (n - 1)), palette)),
                    topLeft = Offset(i * w, 0f), size = Size(w + 1f, size.height))
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("weak", fontSize = 10.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text("strong", fontSize = 10.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }

        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Palette.entries.forEach { p ->
                FilterChip(selected = palette == p, onClick = { onPalette(p) }, label = { Text(p.label) })
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = { Haptics.medium(view); RadioRepository.clearWaterfall(); RadioRepository.send(Protocol.waterfallStart()) },
                modifier = Modifier.weight(1f),
            ) { Text("Start") }
            OutlinedButton(
                onClick = { Haptics.light(view); RadioRepository.send(Protocol.waterfallStop()) },
                modifier = Modifier.weight(1f),
            ) { Text("Stop") }
            OutlinedButton(
                onClick = { Haptics.light(view); RadioRepository.clearWaterfall() },
                modifier = Modifier.weight(1f),
            ) { Text("Clear") }
        }
    }
}

/**
 * Render the waterfall rows into a single [ImageBitmap] via setPixels — far cheaper
 * than drawing one rect per cell every frame. Newest row sits at the top (y = 0).
 */
private fun buildWaterfall(rows: List<IntArray>, palette: Palette): ImageBitmap? {
    if (rows.isEmpty()) return null
    val w = rows.maxOf { it.size }
    if (w == 0) return null
    val h = rows.size
    val pixels = IntArray(w * h)
    for (y in 0 until h) {
        val src = rows[h - 1 - y]          // top = newest
        val base = y * w
        for (x in 0 until w) {
            val v = if (x < src.size) src[x] else if (src.isNotEmpty()) src[src.size - 1] else 0
            pixels[base + x] = paletteArgb(v, palette)
        }
    }
    val bmp = android.graphics.Bitmap.createBitmap(w, h, android.graphics.Bitmap.Config.ARGB_8888)
    bmp.setPixels(pixels, 0, w, 0, 0, w, h)
    return bmp.asImageBitmap()
}

/** Map a 0..255 intensity to an ARGB color in the chosen [palette]. */
private fun paletteArgb(value: Int, palette: Palette): Int {
    val t = (value.coerceIn(0, 255)) / 255f
    val (r, g, b) = when (palette) {
        Palette.GRAY -> Triple(t, t, t)
        Palette.CLASSIC -> when {                       // blue → cyan → yellow → red
            t < 0.33f -> Triple(0f, t * 3f, 0.5f + t)
            t < 0.66f -> Triple((t - 0.33f) * 3f, 1f, 1f - (t - 0.33f) * 3f)
            else -> Triple(1f, 1f - (t - 0.66f) * 3f, 0f)
        }
        Palette.VIRIDIS -> Triple(                       // perceptual purple → green → yellow
            (0.28f + 0.5f * t).coerceIn(0f, 1f),
            (t * t * 0.4f + t * 0.6f).coerceIn(0f, 1f),
            (0.35f + 0.6f * (1f - t) * t * 4f).coerceIn(0f, 1f),
        )
        Palette.TURBO -> Triple(                          // dark → blue → green → yellow → red
            (1.8f * t - 0.6f).coerceIn(0f, 1f),
            (kotlin.math.sin(t * Math.PI).toFloat()).coerceIn(0f, 1f),
            (1f - 1.8f * t + 0.2f).coerceIn(0f, 1f),
        )
    }
    val ri = (r * 255).toInt().coerceIn(0, 255)
    val gi = (g * 255).toInt().coerceIn(0, 255)
    val bi = (b * 255).toInt().coerceIn(0, 255)
    return (0xFF shl 24) or (ri shl 16) or (gi shl 8) or bi
}
