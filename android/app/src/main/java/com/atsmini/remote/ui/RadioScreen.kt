package com.atsmini.remote.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.VolumeUp
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material.icons.filled.Bedtime
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import kotlin.math.roundToInt
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.atsmini.remote.data.Protocol
import com.atsmini.remote.data.RadioOptions
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.data.RadioStatus
import com.atsmini.remote.ui.components.Haptics
import com.atsmini.remote.ui.components.SectionCard
import com.atsmini.remote.ui.components.SpectrumChart

@Composable
fun RadioScreen(modifier: Modifier = Modifier) {
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    Column(
        modifier = modifier.fillMaxWidth().padding(horizontal = 14.dp, vertical = 8.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        FrequencyCard(status)
        ControlsCard(status)
        SpectrumCard(status)
        SignalCard(status)
    }
}

@Composable
private fun SpectrumCard(status: RadioStatus) {
    val scan by RadioRepository.scan.collectAsStateWithLifecycle()
    val progress by RadioRepository.scanProgress.collectAsStateWithLifecycle()
    val view = LocalView.current
    SectionCard(title = "Spectrum") {
        // Compact, controls-free chart: the tuned-frequency marker tracks the live
        // status, so it slides in real time as you tune above. Drag to inspect bins.
        SpectrumChart(
            scan = scan,
            tunedFreq = status.frequency,
            isFM = status.isFM,
            progress = progress,
            height = 150.dp,
            showControls = false,
            onScan = { Haptics.medium(view); RadioRepository.send(Protocol.scan(1)) },
        )
    }
}

@Composable
private fun FrequencyCard(status: RadioStatus) {
    SectionCard {
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(status.bandName, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(status.modeName, color = MaterialTheme.colorScheme.secondary, fontWeight = FontWeight.SemiBold)
            }
            Row(verticalAlignment = Alignment.Bottom) {
                Text(
                    status.formattedFrequency(com.atsmini.remote.data.FreqUnit.AUTO),
                    fontSize = 52.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                )
                Text(
                    "  ${status.frequencyUnitLabel(com.atsmini.remote.data.FreqUnit.AUTO)}",
                    fontSize = 18.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(bottom = 8.dp),
                )
            }
            if (status.rdsStation.isNotEmpty()) {
                Text(status.rdsStation, fontWeight = FontWeight.SemiBold)
            }
            if (status.rdsText.isNotEmpty()) {
                Text(status.rdsText, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
            }
        }
    }
}

@Composable
private fun ControlsCard(status: RadioStatus) {
    val view = LocalView.current
    val opts by RadioRepository.options.collectAsStateWithLifecycle()
    SectionCard(title = "Tuning") {
        // Big tune / seek row
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TuneButton("◁◁", Modifier.weight(1f)) { Haptics.medium(view); RadioRepository.send(Protocol.seek(-1)) }
            TuneButton("−", Modifier.weight(1f)) { Haptics.light(view); RadioRepository.send(Protocol.rotateDown()) }
            TuneButton("+", Modifier.weight(1f)) { Haptics.light(view); RadioRepository.send(Protocol.rotateUp()) }
            TuneButton("▷▷", Modifier.weight(1f)) { Haptics.medium(view); RadioRepository.send(Protocol.seek(1)) }
        }

        // Menu pickers (Band / Mode / Step / BW / AGC) populated from the device.
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            MenuPicker("Band", status.bandName, Dim.BAND, opts, status, Modifier.weight(1f))
            MenuPicker("Mode", status.modeName, Dim.MODE, opts, status, Modifier.weight(1f))
            MenuPicker("Step", status.stepSize, Dim.STEP, opts, status, Modifier.weight(1f))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            MenuPicker("BW", status.bandwidth, Dim.BW, opts, status, Modifier.weight(1f))
            MenuPicker("AGC", status.agc, Dim.AGC, opts, status, Modifier.weight(1f))
        }

        // Volume — single draggable slider. While dragging we hold a local value so
        // incoming status frames don't fight the thumb; on release we send the radio
        // a single relative step equal to (target − current), which it clamps.
        var dragging by remember { mutableStateOf(false) }
        var localVol by remember { mutableFloatStateOf(status.volume.toFloat()) }
        if (!dragging) localVol = status.volume.toFloat()
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.AutoMirrored.Filled.VolumeUp, contentDescription = "Volume",
                tint = MaterialTheme.colorScheme.onSurfaceVariant)
            Slider(
                value = localVol,
                onValueChange = { dragging = true; localVol = it },
                onValueChangeFinished = {
                    val target = localVol.roundToInt()
                    val delta = target - status.volume
                    if (delta != 0) { Haptics.light(view); RadioRepository.send(Protocol.volume(delta)) }
                    dragging = false
                },
                valueRange = 0f..100f,
                enabled = status.isConnected,
                modifier = Modifier.weight(1f).padding(horizontal = 10.dp),
            )
            Text("${localVol.roundToInt()}", fontWeight = FontWeight.SemiBold,
                modifier = Modifier.width(32.dp))
        }

        // Sleep
        FilledTonalButton(
            onClick = { Haptics.heavy(view); RadioRepository.send(Protocol.sleep(true)) },
            modifier = Modifier.fillMaxWidth(),
        ) {
            Icon(Icons.Filled.Bedtime, contentDescription = null)
            Text("  Sleep")
        }
    }
}

/** A selectable radio dimension and the delta command that changes it. */
private enum class Dim(val cmd: String) { BAND("band"), MODE("mode"), STEP("step"), BW("bw"), AGC("agc") }

/** Resolve the option labels for [dim] from the device options snapshot. */
private fun dimOptions(dim: Dim, opts: RadioOptions?): List<String> = when (dim) {
    Dim.BAND -> opts?.band?.options ?: emptyList()
    Dim.MODE -> opts?.mode?.options ?: emptyList()
    Dim.STEP -> opts?.step?.options ?: emptyList()
    Dim.BW -> opts?.bw?.options ?: emptyList()
    Dim.AGC -> opts?.let { (0..it.agcMax).map { n -> n.toString() } } ?: emptyList()
}

/**
 * Compact menu chip that opens a dropdown of the real radio options. Selecting an
 * item jumps the radio there by sending a single relative step = (target − current);
 * the current index is read from the live status value so it never drifts. Falls
 * back to up/down stepping until the option list has arrived (or if the live value
 * can't be matched). Options are fetched on connect and re-fetched after a band or
 * mode change, so this adds no steady-state traffic or per-frame work.
 */
@Composable
private fun MenuPicker(
    label: String, value: String, dim: Dim, opts: RadioOptions?, status: RadioStatus,
    modifier: Modifier = Modifier,
) {
    var expanded by remember { mutableStateOf(false) }
    val view = LocalView.current
    val list = dimOptions(dim, opts)
    val currentIdx = if (dim == Dim.AGC) value.toIntOrNull() ?: -1 else list.indexOf(value)

    Box(modifier) {
        AssistChip(
            onClick = { if (opts == null) RadioRepository.requestOptions(); expanded = true },
            enabled = status.isConnected,
            label = {
                Column {
                    Text(label, fontSize = 10.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text(value, fontWeight = FontWeight.SemiBold, maxLines = 1)
                }
            },
            modifier = Modifier.fillMaxWidth(),
        )
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            if (list.isEmpty() || currentIdx < 0) {
                // Fallback: relative stepping until options are known / value matches.
                DropdownMenuItem(
                    text = { Text("$label up") },
                    leadingIcon = { Icon(Icons.Filled.KeyboardArrowUp, null) },
                    onClick = { Haptics.light(view); RadioRepository.send(Protocol.delta(dim.cmd, 1)); expanded = false },
                )
                DropdownMenuItem(
                    text = { Text("$label down") },
                    leadingIcon = { Icon(Icons.Filled.KeyboardArrowDown, null) },
                    onClick = { Haptics.light(view); RadioRepository.send(Protocol.delta(dim.cmd, -1)); expanded = false },
                )
            } else {
                list.forEachIndexed { idx, name ->
                    DropdownMenuItem(
                        text = {
                            Text(name, fontWeight = if (idx == currentIdx) FontWeight.Bold else FontWeight.Normal,
                                color = if (idx == currentIdx) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface)
                        },
                        onClick = {
                            expanded = false
                            // FM band is mode-locked: the radio ignores switching into/out of FM.
                            if (dim == Dim.MODE && (name == "FM" || status.isFM)) return@DropdownMenuItem
                            val delta = idx - currentIdx
                            if (delta != 0) {
                                Haptics.light(view)
                                RadioRepository.send(Protocol.delta(dim.cmd, delta))
                                // Step/BW lists depend on band & mode — refresh after a change.
                                if (dim == Dim.BAND || dim == Dim.MODE) RadioRepository.requestOptions()
                            }
                        },
                    )
                }
            }
        }
    }
}

@Composable
private fun TuneButton(label: String, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Button(
        onClick = onClick,
        modifier = modifier.height(48.dp),
        shape = RoundedCornerShape(16.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = MaterialTheme.colorScheme.primary,
            contentColor = MaterialTheme.colorScheme.onPrimary,
        ),
    ) { Text(label, fontWeight = FontWeight.Bold) }
}

@Composable
private fun SignalCard(status: RadioStatus) {
    SectionCard(title = "Signal") {
        Meter("RSSI", status.rssi, 127, "dBμV")
        Meter("SNR", status.snr, 40, "dB")
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Battery ${String.format("%.2f", status.batteryVoltage)} V",
                color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
            Text("CPU ${status.cpu0}% / ${status.cpu1}%",
                color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
        }
    }
}

@Composable
private fun Meter(label: String, value: Int, max: Int, unit: String) {
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text("$value $unit", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
        }
        LinearProgressIndicator(
            progress = { (value.toFloat() / max).coerceIn(0f, 1f) },
            modifier = Modifier.fillMaxWidth().height(8.dp),
            color = MaterialTheme.colorScheme.secondary,
        )
    }
}
