package com.atsmini.remote.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.atsmini.remote.data.Protocol
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.ui.components.Haptics
import com.atsmini.remote.ui.components.SectionCard
import com.atsmini.remote.ui.components.SpectrumChart
import com.atsmini.remote.ui.components.fmtFreq

/** Available waterfall/intensity color palettes (common SDR choices). */
private enum class Palette(val label: String) { TURBO("Turbo"), VIRIDIS("Viridis"), CLASSIC("Classic"), GRAY("Gray") }

@Composable
fun VisualizeScreen(modifier: Modifier = Modifier) {
    var tab by remember { mutableIntStateOf(0) }
    var palette by remember { mutableStateOf(Palette.TURBO) }

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
        if (tab == 0) SpectrumCard()
        else WaterfallCard(palette, { palette = it })
    }
}

// ---------------------------------------------------------------------------
// Spectrum — thin wrapper over the shared interactive SpectrumChart.
// ---------------------------------------------------------------------------

@Composable
private fun SpectrumCard() {
    val scan by RadioRepository.scan.collectAsStateWithLifecycle()
    val progress by RadioRepository.scanProgress.collectAsStateWithLifecycle()
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    val view = LocalView.current

    SectionCard(title = "Spectrum") {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val h = if (maxWidth >= 600.dp) 300.dp else 220.dp
            SpectrumChart(
                scan = scan,
                tunedFreq = status.frequency,
                isFM = status.isFM,
                progress = progress,
                height = h,
                showControls = true,
                onScan = { Haptics.medium(view); RadioRepository.send(Protocol.scan(1)) },
            )
        }
    }
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
