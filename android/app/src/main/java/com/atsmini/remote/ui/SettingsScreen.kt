package com.atsmini.remote.ui

import android.os.Build
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.atsmini.remote.Controllers
import com.atsmini.remote.data.Protocol
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.ui.components.SectionCard
import com.atsmini.remote.ui.theme.AppTheme
import com.atsmini.remote.ui.theme.ThemeController

@Composable
fun SettingsScreen(modifier: Modifier = Modifier) {
    // No page scroll: fixed cards top and bottom, the variable presets list takes
    // the remaining space and scrolls internally.
    Column(
        modifier = modifier.fillMaxSize().padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        ConnectionCard()
        ThemeCard()
        PresetsCard(Modifier.weight(1f))
        AboutCard()
    }
}

@Composable
private fun ConnectionCard() {
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    val scanning by Controllers.ble.scanning.collectAsStateWithLifecycle()
    val devices by Controllers.ble.devices.collectAsStateWithLifecycle()
    SectionCard(title = "Connection") {
        Text(status.connectionStatus, fontWeight = FontWeight.SemiBold,
            color = if (status.isConnected) MaterialTheme.colorScheme.secondary else MaterialTheme.colorScheme.onSurfaceVariant)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { Controllers.ble.startScan() }, enabled = !scanning, modifier = Modifier.weight(1f)) {
                Text(if (scanning) "Scanning…" else "Scan BLE")
            }
            OutlinedButton(onClick = { Controllers.ble.disconnect(); Controllers.usb.close() }, modifier = Modifier.weight(1f)) {
                Text("Disconnect")
            }
        }
        devices.forEach { dev ->
            HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.3f))
            Row(
                Modifier.fillMaxWidth().clickable { Controllers.ble.connect(dev.address) }.padding(vertical = 6.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(dev.name)
                Text("Connect", color = MaterialTheme.colorScheme.primary, fontSize = 13.sp)
            }
        }
    }
}

@Composable
private fun ThemeCard() {
    val theme by ThemeController.theme.collectAsStateWithLifecycle()
    val dynamic by ThemeController.dynamic.collectAsStateWithLifecycle()
    SectionCard(title = "Appearance") {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Column {
                    Text("Material You", fontWeight = FontWeight.SemiBold)
                    Text("Use system wallpaper colors", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                Switch(checked = dynamic, onCheckedChange = { ThemeController.setDynamic(it) })
            }
            HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.3f))
        }
        Text(
            if (dynamic) "Dynamic color active — pick a palette below to switch back." else "Theme: ${theme.displayName}",
            fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        LazyVerticalGrid(
            columns = GridCells.Fixed(6),
            modifier = Modifier.fillMaxWidth().height(140.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            items(AppTheme.entries) { t ->
                val selected = !dynamic && t == theme
                Box(
                    Modifier
                        .size(40.dp)
                        .background(t.backgroundColor, CircleShape)
                        .border(
                            width = if (selected) 3.dp else 1.dp,
                            color = if (selected) MaterialTheme.colorScheme.primary else Color.Gray.copy(alpha = 0.4f),
                            shape = CircleShape,
                        )
                        .clickable { ThemeController.setDynamic(false); ThemeController.setTheme(t) },
                    contentAlignment = Alignment.Center,
                ) {
                    Box(Modifier.size(16.dp).background(t.primaryColor, CircleShape))
                }
            }
        }
    }
}

// Built from a raw Card (not SectionCard) so the preset list can take weight(1f)
// of the card height and scroll internally — keeping the Settings page fixed.
@Composable
private fun PresetsCard(modifier: Modifier = Modifier) {
    val presets by RadioRepository.presets.collectAsStateWithLifecycle()
    Card(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(24.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface,
            contentColor = MaterialTheme.colorScheme.onSurface,
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 0.dp),
    ) {
        Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically) {
                Text("Presets", style = MaterialTheme.typography.labelLarge, fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                TextButton(onClick = { RadioRepository.send(Protocol.listPresets()) }) { Text("Refresh") }
            }
            if (presets.isEmpty()) {
                Text("No presets saved", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
            }
            Column(Modifier.fillMaxWidth().weight(1f).verticalScroll(rememberScrollState())) {
                presets.forEach { p ->
                    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                        Column {
                            Text(p.name, fontWeight = FontWeight.SemiBold)
                            Text("${p.channelCount} channels", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                        Row {
                            TextButton(onClick = { RadioRepository.send(Protocol.loadPreset(p.idx)) }) { Text("Load") }
                            TextButton(onClick = { RadioRepository.send(Protocol.deletePreset(p.idx)) }) { Text("Delete") }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun AboutCard() {
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    SectionCard(title = "About") {
        InfoRow("App", com.atsmini.remote.BuildConfig.VERSION_NAME)
        InfoRow("Firmware", if (status.firmwareVersion > 0) "v${status.firmwareVersion / 100.0}" else "—")
        InfoRow("Radio IP", status.wifiIP.ifEmpty { "—" })
        Text(
            "Sideloaded build. Updates come straight from GitHub Releases via Obtainium.",
            fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun InfoRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
        Text(value, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
    }
}
