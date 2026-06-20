package com.atsmini.remote.ui

import android.os.Handler
import android.os.Looper
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.ContentCopy
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.atsmini.remote.Controllers
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.data.Transport
import com.atsmini.remote.net.GithubReleases
import com.atsmini.remote.net.RecoveryOta
import com.atsmini.remote.shizuku.ShizukuManager
import com.atsmini.remote.ui.components.SectionCard
import com.atsmini.remote.usb.EspFlasher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private val mainHandler = Handler(Looper.getMainLooper())

@Composable
fun ToolsScreen(onRequestUsb: () -> Unit, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier.fillMaxWidth().padding(14.dp).verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        ShizukuCard()
        UsbConsoleCard(onRequestUsb)
        FlashCard()
        RecoveryOtaCard()
    }
}

@Composable
private fun ShizukuCard() {
    val available by ShizukuManager.available.collectAsStateWithLifecycle()
    val granted by ShizukuManager.granted.collectAsStateWithLifecycle()
    SectionCard(title = "Shizuku (privileged helper)") {
        Text(
            when {
                !available -> "Not running. Start Shizuku to unlock silent USB permission, auto-join recovery Wi-Fi, and radio auto-toggle."
                granted -> "Connected — ${ShizukuManager.version()}"
                else -> "Running — permission needed."
            },
            color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp,
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { ShizukuManager.requestPermission() }, enabled = available && !granted,
                modifier = Modifier.weight(1f)) { Text("Grant") }
            OutlinedButton(onClick = { ShizukuManager.refresh() }, modifier = Modifier.weight(1f)) { Text("Refresh") }
        }
        if (granted) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { ShizukuManager.enableBluetooth() }, modifier = Modifier.weight(1f)) { Text("BT on") }
                OutlinedButton(onClick = { ShizukuManager.enableWifi() }, modifier = Modifier.weight(1f)) { Text("Wi-Fi on") }
            }
        }
    }
}

@Composable
private fun UsbConsoleCard(onRequestUsb: () -> Unit) {
    val console by RadioRepository.console.collectAsStateWithLifecycle()
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    var autoScroll by remember { mutableStateOf(true) }
    val clipboardManager = LocalClipboardManager.current
    val isUsbConnected = status.transport == Transport.USB

    SectionCard(
        title = "USB serial console",
        trailing = {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("Auto-scroll", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Switch(checked = autoScroll, onCheckedChange = { autoScroll = it })
            }
        },
    ) {
        Text(
            "Wired link to the radio. Captures the ESP32 boot log and panic traces — useful for diagnosing a bootloop.",
            color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp,
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = onRequestUsb,
                enabled = !isUsbConnected,
                modifier = Modifier.weight(1f),
            ) { Text("Connect USB") }
            OutlinedButton(
                onClick = { Controllers.usb.close() },
                enabled = isUsbConnected,
                modifier = Modifier.weight(1f),
            ) { Text("Disconnect") }
            OutlinedButton(
                onClick = { RadioRepository.clearConsole() },
                modifier = Modifier.weight(1f),
            ) { Text("Clear") }
        }
        val scroll = rememberScrollState()
        LaunchedEffect(console, autoScroll) {
            if (autoScroll) scroll.scrollTo(scroll.maxValue)
        }
        Box(Modifier.fillMaxWidth()) {
            Text(
                console.takeLast(8000).ifEmpty { "(no data)" },
                fontFamily = FontFamily.Monospace, fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurface,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(200.dp)
                    .verticalScroll(scroll)
                    .padding(end = 40.dp),
            )
            IconButton(
                onClick = { clipboardManager.setText(AnnotatedString(console.ifEmpty { "(no data)" })) },
                modifier = Modifier.align(Alignment.TopEnd),
            ) {
                Icon(
                    Icons.Outlined.ContentCopy,
                    contentDescription = "Copy logs to clipboard",
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun FlashCard() {
    val scope = rememberCoroutineScope()
    var busy by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("Idle") }
    var pct by remember { mutableIntStateOf(0) }

    SectionCard(title = "USB firmware flash (recovery)") {
        Text(
            "Downloads the latest firmware and writes the full 8 MB image over USB. Recovers a bricked or bootlooping radio with no PC.",
            color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp,
        )
        Text(
            "Bootloader mode: hold BOOT, press and release RESET, then release BOOT. The USB port stays connected at the same address.",
            color = MaterialTheme.colorScheme.tertiary, fontSize = 12.sp,
        )
        Text(statusText, fontSize = 13.sp)
        if (busy) LinearProgressIndicator(progress = { pct / 100f }, modifier = Modifier.fillMaxWidth())
        Button(
            enabled = !busy,
            modifier = Modifier.fillMaxWidth(),
            onClick = {
                busy = true; pct = 0; statusText = "Looking up latest firmware…"
                scope.launch(Dispatchers.IO) {
                    val fw = GithubReleases.latestFirmware()
                    val asset = fw?.full() ?: fw?.flash()
                    if (asset == null) { post { statusText = "No firmware asset found"; busy = false }; return@launch }
                    post { statusText = "Downloading ${asset.name}…" }
                    val bytes = GithubReleases.download(asset.url) { p -> post { pct = p } }
                    if (bytes == null) { post { statusText = "Download failed"; busy = false }; return@launch }
                    val port = Controllers.usb.takePortForFlashing()
                    if (port == null) { post { statusText = "Connect USB & enter bootloader mode first"; busy = false }; return@launch }
                    val ok = EspFlasher(port).flash(0x0, bytes, object : EspFlasher.Progress {
                        override fun status(message: String) { post { statusText = message } }
                        override fun percent(value: Int) { post { pct = value } }
                    })
                    Controllers.usb.resumeAfterFlash()
                    post { statusText = if (ok) "Flashed — radio rebooting" else statusText; busy = false }
                }
            },
        ) { Text("Download latest & flash over USB") }
    }
}

@Composable
private fun RecoveryOtaCard() {
    val scope = rememberCoroutineScope()
    val shizukuGranted by ShizukuManager.granted.collectAsStateWithLifecycle()
    var busy by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("Idle") }
    var pct by remember { mutableIntStateOf(0) }

    SectionCard(title = "Recovery AP firmware update") {
        Text(
            "Auto-joins the radio's \"ATS-Mini Recovery\" Wi-Fi (hold the encoder at power-on) and uploads firmware over the air — one tap with Shizuku.",
            color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp,
        )
        Text(statusText, fontSize = 13.sp)
        if (busy) LinearProgressIndicator(progress = { pct / 100f }, modifier = Modifier.fillMaxWidth())
        Button(
            enabled = !busy,
            modifier = Modifier.fillMaxWidth(),
            onClick = {
                busy = true; pct = 0; statusText = "Joining recovery Wi-Fi…"
                scope.launch(Dispatchers.IO) {
                    if (shizukuGranted) {
                        ShizukuManager.connectWifi("ATS-Mini Recovery", "atsrecover")
                        delay(8000)
                    } else {
                        post { statusText = "Connect to 'ATS-Mini Recovery' Wi-Fi manually, then continue…" }
                        delay(4000)
                    }
                    val fw = GithubReleases.latestFirmware()
                    val asset = fw?.ota()
                    if (asset == null) { post { statusText = "No OTA asset found"; busy = false }; return@launch }
                    post { statusText = "Downloading ${asset.name}…" }
                    val bytes = GithubReleases.download(asset.url) { p -> post { pct = p } }
                    if (bytes == null) { post { statusText = "Download failed"; busy = false }; return@launch }
                    val ok = RecoveryOta.upload(RecoveryOta.RECOVERY_HOST, bytes, object : RecoveryOta.Progress {
                        override fun status(message: String) { post { statusText = message } }
                        override fun percent(value: Int) { post { pct = value } }
                    })
                    post { busy = false; if (!ok && statusText == "Idle") statusText = "Upload failed" }
                }
            },
        ) { Text("Auto-join recovery AP & upload OTA") }
    }
}

private fun post(block: () -> Unit) = mainHandler.post(block)
