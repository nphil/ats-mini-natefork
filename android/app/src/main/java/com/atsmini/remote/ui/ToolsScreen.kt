package com.atsmini.remote.ui

import android.content.Context
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts.OpenDocument
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.ContentCopy
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private val mainHandler = Handler(Looper.getMainLooper())
private fun post(block: () -> Unit) = mainHandler.post(block)

/** Read a URI via ContentResolver, returning (displayName, bytes) or null on failure. */
private fun readUri(context: Context, uri: Uri): Pair<String, ByteArray>? {
    val name = runCatching {
        context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (cursor.moveToFirst() && idx >= 0) cursor.getString(idx) else null
        }
    }.getOrNull() ?: uri.lastPathSegment ?: "unknown.bin"
    val bytes = runCatching {
        context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
    }.getOrNull() ?: return null
    return name to bytes
}

/**
 * Single-screen Tools tab — no page scrolling. A persistent device-status header
 * sits above a segmented switch between three panels (USB flash / Wi-Fi OTA /
 * Console). The flashing coroutine and its progress live at this level, so an
 * in-flight flash survives switching panels and is always visible in the strip.
 */
@Composable
fun ToolsScreen(onRequestUsb: () -> Unit, modifier: Modifier = Modifier) {
    val scope = rememberCoroutineScope()
    var busy by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("") }
    var pct by remember { mutableIntStateOf(0) }
    var panel by remember { mutableIntStateOf(0) }

    val ctl = remember {
        OpControls(
            scope = scope,
            setBusy = { v -> post { busy = v } },
            setStatus = { s -> post { statusText = s } },
            setPct = { p -> post { pct = p } },
        )
    }

    Column(
        modifier = modifier.fillMaxSize().padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        DeviceHeader(onRequestUsb)

        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            SegmentedButton(selected = panel == 0, onClick = { panel = 0 },
                shape = SegmentedButtonDefaults.itemShape(0, 3)) { Text("USB flash") }
            SegmentedButton(selected = panel == 1, onClick = { panel = 1 },
                shape = SegmentedButtonDefaults.itemShape(1, 3)) { Text("Wi-Fi OTA") }
            SegmentedButton(selected = panel == 2, onClick = { panel = 2 },
                shape = SegmentedButtonDefaults.itemShape(2, 3)) { Text("Console") }
        }

        if (busy || statusText.isNotEmpty()) {
            SectionCard {
                Text(statusText.ifEmpty { "Idle" }, fontSize = 13.sp, fontWeight = FontWeight.Medium)
                if (busy) LinearProgressIndicator(
                    progress = { pct / 100f },
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }

        Box(Modifier.weight(1f).fillMaxWidth()) {
            when (panel) {
                0 -> UsbFlashPanel(busy, ctl)
                1 -> WifiOtaPanel(busy, ctl)
                else -> ConsolePanel(onRequestUsb)
            }
        }
    }
}

/** Shared operation context handed to panels so coroutines/progress are hoisted. */
private class OpControls(
    val scope: CoroutineScope,
    val setBusy: (Boolean) -> Unit,
    val setStatus: (String) -> Unit,
    val setPct: (Int) -> Unit,
) {
    val flashCb = object : EspFlasher.Progress {
        override fun status(message: String) = setStatus(message)
        override fun percent(value: Int) = setPct(value)
    }
    val otaCb = object : RecoveryOta.Progress {
        override fun status(message: String) = setStatus(message)
        override fun percent(value: Int) = setPct(value)
    }
}

@Composable
private fun DeviceHeader(onRequestUsb: () -> Unit) {
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    val shizukuAvail by ShizukuManager.available.collectAsStateWithLifecycle()
    val shizukuGranted by ShizukuManager.granted.collectAsStateWithLifecycle()
    val usbConnected = status.transport == Transport.USB

    SectionCard(title = "Device") {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("USB serial", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(
                    if (usbConnected) "Connected" else "Not connected",
                    fontWeight = FontWeight.SemiBold,
                    color = if (usbConnected) MaterialTheme.colorScheme.secondary
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            if (usbConnected) OutlinedButton(onClick = { Controllers.usb.close() }) { Text("Disconnect") }
            else Button(onClick = onRequestUsb) { Text("Connect") }
        }
        HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.3f))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("Shizuku helper", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(
                    when {
                        !shizukuAvail -> "Not running"
                        shizukuGranted -> "Connected — auto-join recovery Wi-Fi"
                        else -> "Permission needed"
                    },
                    fontWeight = FontWeight.SemiBold,
                    fontSize = 13.sp,
                    color = if (shizukuGranted) MaterialTheme.colorScheme.secondary
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            if (shizukuAvail && !shizukuGranted)
                Button(onClick = { ShizukuManager.requestPermission() }) { Text("Grant") }
            else
                OutlinedButton(onClick = { ShizukuManager.refresh() }) { Text("Refresh") }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun UsbFlashPanel(busy: Boolean, ctl: OpControls) {
    val context = LocalContext.current

    var useGitHub by remember { mutableStateOf(true) }
    var useFullImage by remember { mutableStateOf(true) }

    var releases by remember { mutableStateOf<List<GithubReleases.Firmware>>(emptyList()) }
    var releasesLoading by remember { mutableStateOf(false) }
    var releasesError by remember { mutableStateOf(false) }
    var selectedRelease by remember { mutableStateOf<GithubReleases.Firmware?>(null) }
    var dropdownExpanded by remember { mutableStateOf(false) }
    var loadTrigger by remember { mutableIntStateOf(0) }

    var fileName by remember { mutableStateOf<String?>(null) }
    var fileBytes by remember { mutableStateOf<ByteArray?>(null) }

    val picker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        ctl.scope.launch(Dispatchers.IO) {
            val r = readUri(context, uri)
            post { fileName = r?.first; fileBytes = r?.second }
        }
    }

    LaunchedEffect(loadTrigger, useGitHub) {
        if (!useGitHub) return@LaunchedEffect
        if (releases.isNotEmpty()) return@LaunchedEffect
        releasesLoading = true; releasesError = false
        val result = kotlinx.coroutines.withContext(Dispatchers.IO) { GithubReleases.listFirmware(15) }
        releasesLoading = false
        if (result.isEmpty()) releasesError = true
        else { releases = result; if (selectedRelease == null) selectedRelease = result.firstOrNull() }
    }

    val selectedAsset = if (useGitHub)
        selectedRelease?.let { if (useFullImage) it.full() else it.flash() } else null
    val canFlash = !busy && if (useGitHub) selectedAsset != null else fileBytes != null

    SectionCard(title = "Flash over USB cable") {
        Text("Recovers a bricked or bootlooping radio with no PC. Put the radio in " +
            "bootloader mode: hold BOOT, tap RESET, release BOOT.",
            fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(selected = useGitHub, onClick = { useGitHub = true },
                label = { Text("GitHub") })
            FilterChip(selected = !useGitHub, onClick = { useGitHub = false },
                label = { Text("Local file") })
        }

        if (useGitHub) {
            when {
                releasesLoading -> Row(verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    CircularProgressIndicator(Modifier.height(20.dp))
                    Text("Loading releases…", fontSize = 13.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                releasesError -> {
                    Text("Failed to load releases.", fontSize = 13.sp, color = MaterialTheme.colorScheme.error)
                    OutlinedButton(onClick = { releasesError = false; loadTrigger++ }) { Text("Retry") }
                }
                else -> {
                    ExposedDropdownMenuBox(expanded = dropdownExpanded,
                        onExpandedChange = { dropdownExpanded = it }, modifier = Modifier.fillMaxWidth()) {
                        OutlinedTextField(
                            value = selectedRelease?.displayName ?: "Select release…",
                            onValueChange = {}, readOnly = true,
                            label = { Text("Firmware version") },
                            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = dropdownExpanded) },
                            modifier = Modifier.fillMaxWidth()
                                .menuAnchor(MenuAnchorType.PrimaryNotEditable, true),
                        )
                        ExposedDropdownMenu(
                            expanded = dropdownExpanded, onDismissRequest = { dropdownExpanded = false }) {
                            releases.forEach { fw ->
                                DropdownMenuItem(text = { Text(fw.displayName) },
                                    onClick = { selectedRelease = fw; dropdownExpanded = false })
                            }
                        }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilterChip(selected = useFullImage, onClick = { useFullImage = true },
                            label = { Text("Full 8 MB") })
                        FilterChip(selected = !useFullImage, onClick = { useFullImage = false },
                            label = { Text("Flash (keep data)") })
                    }
                    if (selectedAsset != null) Text("${selectedAsset.name}  (${selectedAsset.size / 1024} KB)",
                        fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant,
                        maxLines = 1, overflow = TextOverflow.Ellipsis)
                    else if (selectedRelease != null) Text("Not available for this release",
                        fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
                }
            }
        } else {
            FilePickerRow(label = "Firmware .bin (full or flash image)", fileName = fileName,
                onPick = { picker.launch(arrayOf("application/octet-stream", "*/*")) })
        }

        Button(enabled = canFlash, modifier = Modifier.fillMaxWidth(), onClick = {
            ctl.setBusy(true); ctl.setStatus("Preparing…"); ctl.setPct(0)
            ctl.scope.launch(Dispatchers.IO) {
                try {
                    val bytes = if (useGitHub) {
                        val asset = selectedAsset ?: run { ctl.setStatus("No asset available"); return@launch }
                        ctl.setStatus("Downloading ${asset.name}…")
                        GithubReleases.download(asset.url) { ctl.setPct(it) }
                            ?: run { ctl.setStatus("Download failed"); return@launch }
                    } else fileBytes ?: run { ctl.setStatus("No file selected"); return@launch }

                    val port = Controllers.usb.takePortForFlashing()
                        ?: run { ctl.setStatus("Connect USB & enter bootloader mode first"); return@launch }
                    val ok = EspFlasher(port).flash(0x0, bytes, ctl.flashCb)
                    Controllers.usb.resumeAfterFlash()
                    if (ok) ctl.setStatus("Flashed — radio rebooting")
                } finally { ctl.setBusy(false) }
            }
        }) { Text(if (useGitHub) "Download & flash over USB" else "Flash file over USB") }
    }
}

@Composable
private fun WifiOtaPanel(busy: Boolean, ctl: OpControls) {
    val context = LocalContext.current
    val shizukuGranted by ShizukuManager.granted.collectAsStateWithLifecycle()

    var useGitHub by remember { mutableStateOf(true) }
    var fileName by remember { mutableStateOf<String?>(null) }
    var fileBytes by remember { mutableStateOf<ByteArray?>(null) }

    val picker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        ctl.scope.launch(Dispatchers.IO) {
            val r = readUri(context, uri)
            post { fileName = r?.first; fileBytes = r?.second }
        }
    }
    val canUpload = !busy && (useGitHub || fileBytes != null)

    SectionCard(title = "Update over recovery Wi-Fi") {
        Text("No cable needed. Boot the radio into recovery (hold the encoder at " +
            "power-on) — it hosts the \"${RecoveryOta.RECOVERY_SSID}\" Wi-Fi. " +
            if (shizukuGranted) "Shizuku joins it for you." else "Join it in Wi-Fi settings first.",
            fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(selected = useGitHub, onClick = { useGitHub = true },
                label = { Text("GitHub latest") })
            FilterChip(selected = !useGitHub, onClick = { useGitHub = false },
                label = { Text("Local file") })
        }
        if (!useGitHub) FilePickerRow(label = "OTA .bin", fileName = fileName,
            onPick = { picker.launch(arrayOf("application/octet-stream", "*/*")) })

        Button(enabled = canUpload, modifier = Modifier.fillMaxWidth(), onClick = {
            ctl.setBusy(true); ctl.setStatus("Joining ${RecoveryOta.RECOVERY_SSID}…"); ctl.setPct(0)
            ctl.scope.launch(Dispatchers.IO) {
                try {
                    if (shizukuGranted) {
                        ShizukuManager.connectWifi(RecoveryOta.RECOVERY_SSID, RecoveryOta.RECOVERY_PASS)
                        delay(8000)
                    } else {
                        ctl.setStatus("Join '${RecoveryOta.RECOVERY_SSID}' Wi-Fi, then wait…")
                        delay(4000)
                    }
                    val bytes = if (useGitHub) {
                        val asset = GithubReleases.latestFirmware()?.ota()
                            ?: run { ctl.setStatus("No OTA asset found"); return@launch }
                        ctl.setStatus("Downloading ${asset.name}…")
                        GithubReleases.download(asset.url) { ctl.setPct(it) }
                    } else fileBytes
                    if (bytes == null) { ctl.setStatus("Download failed"); return@launch }
                    RecoveryOta.upload(RecoveryOta.RECOVERY_HOST, bytes, ctl.otaCb)
                } finally { ctl.setBusy(false) }
            }
        }) { Text(if (shizukuGranted) "Auto-join & upload OTA" else "Upload OTA to recovery") }
    }
}

@Composable
private fun ConsolePanel(onRequestUsb: () -> Unit) {
    val console by RadioRepository.console.collectAsStateWithLifecycle()
    val status by RadioRepository.status.collectAsStateWithLifecycle()
    val clipboard = LocalClipboardManager.current
    val isUsbConnected = status.transport == Transport.USB

    SectionCard(
        modifier = Modifier.fillMaxSize(),
        title = "USB serial console",
        trailing = {
            IconButton(onClick = { clipboard.setText(AnnotatedString(console.ifEmpty { "(no data)" })) }) {
                Icon(Icons.Outlined.ContentCopy, contentDescription = "Copy logs",
                    tint = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        },
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onRequestUsb, enabled = !isUsbConnected, modifier = Modifier.weight(1f)) {
                Text("Connect")
            }
            OutlinedButton(onClick = { Controllers.usb.close() }, enabled = isUsbConnected,
                modifier = Modifier.weight(1f)) { Text("Disconnect") }
            OutlinedButton(onClick = { RadioRepository.clearConsole() }, modifier = Modifier.weight(1f)) {
                Text("Clear")
            }
        }
        val scroll = rememberScrollState()
        LaunchedEffect(console) { scroll.scrollTo(scroll.maxValue) }
        Text(
            console.takeLast(8000).ifEmpty { "Boot log and panic traces appear here once connected." },
            fontFamily = FontFamily.Monospace, fontSize = 10.sp,
            color = MaterialTheme.colorScheme.onSurface,
            modifier = Modifier.fillMaxSize().verticalScroll(scroll),
        )
    }
}

@Composable
private fun FilePickerRow(label: String, fileName: String?, onPick: () -> Unit) {
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Column(Modifier.weight(1f)) {
            Text(label, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(fileName ?: "Not selected", fontSize = 12.sp,
                color = if (fileName != null) MaterialTheme.colorScheme.onSurface
                        else MaterialTheme.colorScheme.outline,
                maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        OutlinedButton(onClick = onPick) { Text("Pick") }
    }
}
