package com.atsmini.remote.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
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
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.data.RadioStatus
import com.atsmini.remote.ui.components.Haptics
import com.atsmini.remote.ui.components.SectionCard

@Composable
fun RadioScreen(modifier: Modifier = Modifier) {
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    Column(
        modifier = modifier.fillMaxWidth().padding(horizontal = 14.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        FrequencyCard(status)
        ControlsCard(status)
        SignalCard(status)
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
    SectionCard(title = "Tuning") {
        // Big tune / seek row
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TuneButton("◁◁", Modifier.weight(1f)) { Haptics.medium(view); RadioRepository.send(Protocol.seek(-1)) }
            TuneButton("−", Modifier.weight(1f)) { Haptics.light(view); RadioRepository.send(Protocol.rotateDown()) }
            TuneButton("+", Modifier.weight(1f)) { Haptics.light(view); RadioRepository.send(Protocol.rotateUp()) }
            TuneButton("▷▷", Modifier.weight(1f)) { Haptics.medium(view); RadioRepository.send(Protocol.seek(1)) }
        }

        // Compact menu pills (Band / Mode / Step / BW / AGC)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            DeltaPill("Band", status.bandName, "band", Modifier.weight(1f))
            DeltaPill("Mode", status.modeName, "mode", Modifier.weight(1f))
            DeltaPill("Step", status.stepSize, "step", Modifier.weight(1f))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            DeltaPill("BW", status.bandwidth, "bw", Modifier.weight(1f))
            DeltaPill("AGC", status.agc, "agc", Modifier.weight(1f))
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

@Composable
private fun DeltaPill(label: String, value: String, cmd: String, modifier: Modifier = Modifier) {
    var expanded by remember { mutableStateOf(false) }
    val view = LocalView.current
    Box(modifier) {
        AssistChip(
            onClick = { expanded = true },
            label = {
                Column {
                    Text(label, fontSize = 10.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text(value, fontWeight = FontWeight.SemiBold, maxLines = 1)
                }
            },
            modifier = Modifier.fillMaxWidth(),
        )
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            DropdownMenuItem(
                text = { Text("$label up") },
                leadingIcon = { Icon(Icons.Filled.KeyboardArrowUp, null) },
                onClick = { Haptics.light(view); RadioRepository.send(Protocol.delta(cmd, 1)); expanded = false },
            )
            DropdownMenuItem(
                text = { Text("$label down") },
                leadingIcon = { Icon(Icons.Filled.KeyboardArrowDown, null) },
                onClick = { Haptics.light(view); RadioRepository.send(Protocol.delta(cmd, -1)); expanded = false },
            )
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
