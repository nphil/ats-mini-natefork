package com.atsmini.remote.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.atsmini.remote.data.Protocol
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.ui.components.Haptics
import com.atsmini.remote.ui.components.SectionCard

@Composable
fun VisualizeScreen(modifier: Modifier = Modifier) {
    var tab by remember { mutableIntStateOf(0) }
    Column(
        modifier = modifier.fillMaxWidth().padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            SegmentedButton(
                selected = tab == 0, onClick = { tab = 0 },
                shape = SegmentedButtonDefaults.itemShape(0, 2),
            ) { Text("Spectrum") }
            SegmentedButton(
                selected = tab == 1, onClick = { tab = 1 },
                shape = SegmentedButtonDefaults.itemShape(1, 2),
            ) { Text("Waterfall") }
        }
        if (tab == 0) SpectrumCard() else WaterfallCard()
    }
}

@Composable
private fun SpectrumCard() {
    val scan by RadioRepository.scan.collectAsStateWithLifecycle()
    val progress by RadioRepository.scanProgress.collectAsStateWithLifecycle()
    val view = LocalView.current
    val primary = MaterialTheme.colorScheme.primary
    val grid = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.2f)

    SectionCard(title = "Spectrum") {
        Canvas(Modifier.fillMaxWidth().height(200.dp)) {
            val data = scan?.rssi ?: return@Canvas
            if (data.isEmpty()) return@Canvas
            val maxV = (data.maxOrNull() ?: 1).coerceAtLeast(1)
            val barW = size.width / data.size
            data.forEachIndexed { i, v ->
                val h = size.height * (v.toFloat() / maxV)
                drawRect(
                    color = primary,
                    topLeft = androidx.compose.ui.geometry.Offset(i * barW, size.height - h),
                    size = androidx.compose.ui.geometry.Size(barW * 0.8f, h),
                )
            }
            drawLine(grid, androidx.compose.ui.geometry.Offset(0f, size.height),
                androidx.compose.ui.geometry.Offset(size.width, size.height))
        }
        if (progress > 0) Text("Scanning… ${progress.toInt()}%", color = MaterialTheme.colorScheme.onSurfaceVariant)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { Haptics.medium(view); RadioRepository.send(Protocol.scan(1)) },
                modifier = Modifier.weight(1f)) { Text("Scan") }
        }
    }
}

@Composable
private fun WaterfallCard() {
    val rows by RadioRepository.waterfall.collectAsStateWithLifecycle()
    val active = RadioRepository.isConnected
    val view = LocalView.current

    SectionCard(title = "Waterfall") {
        Canvas(Modifier.fillMaxWidth().height(240.dp)) {
            if (rows.isEmpty()) return@Canvas
            val rowH = size.height / rows.size
            rows.forEachIndexed { y, row ->
                if (row.isEmpty()) return@forEachIndexed
                val colW = size.width / row.size
                row.forEachIndexed { x, v ->
                    drawRect(
                        color = heat(v),
                        topLeft = androidx.compose.ui.geometry.Offset(x * colW, y * rowH),
                        size = androidx.compose.ui.geometry.Size(colW + 1, rowH + 1),
                    )
                }
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
        }
    }
}

/** Simple blue→cyan→yellow→red heatmap for 0..255 signal values. */
private fun heat(v: Int): Color {
    val t = (v / 255f).coerceIn(0f, 1f)
    return when {
        t < 0.33f -> Color(0f, t * 3f, 0.5f + t)
        t < 0.66f -> Color((t - 0.33f) * 3f, 1f, 1f - (t - 0.33f) * 3f)
        else -> Color(1f, 1f - (t - 0.66f) * 3f, 0f)
    }
}
