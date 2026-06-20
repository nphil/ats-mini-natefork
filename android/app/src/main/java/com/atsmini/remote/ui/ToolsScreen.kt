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
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun FlashCard() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    // Source toggle
    var useGitHub by remember { mutableStateOf(true) }

    // GitHub panel state
    var loadTrigger by remember { mutableIntStateOf(0) }
    var releases by remember { mutableStateOf<List<GithubReleases.Firmware>>(emptyList()) }
    var releasesLoading by remember { mutableStateOf(false) }
    var releasesError by remember { mutableStateOf(false) }
    var selectedRelease by remember { mutableStateOf<GithubReleases.Firmware?>(null) }
    var dropdownExpanded by remember { mutableStateOf(false) }
    var useFullImage by remember { mutableStateOf(true) }  // true = full 8MB, false = flash merged

    // Local panel state
    var localFormatMerged by remember { mutableStateOf(true) }  // true = merged, false = partitions
    var mergedFileName by remember { mutableStateOf<String?>(null) }
    var mergedBytes by remember { mutableStateOf<ByteArray?>(null) }
    var bootFileName by remember { mutableStateOf<String?>(null) }
    var bootBytes by remember { mutableStateOf<ByteArray?>(null) }
    var partTableFileName by remember { mutableStateOf<String?>(null) }
    var partTableBytes by remember { mutableStateOf<ByteArray?>(null) }
    var appFileName by remember { mutableStateOf<String?>(null) }
    var appBytes by remember { mutableStateOf<ByteArray?>(null) }

    // Flash state
    var busy by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("Idle") }
    var pct by remember { mutableIntStateOf(0) }

    // File pickers
    val mergedPicker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        scope.launch(Dispatchers.IO) {
            val result = readUri(context, uri)
            withContext(Dispatchers.Main) {
                mergedFileName = result?.first
                mergedBytes = result?.second
            }
        }
    }
    val bootPicker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        scope.launch(Dispatchers.IO) {
            val result = readUri(context, uri)
            withContext(Dispatchers.Main) {
                bootFileName = result?.first
                bootBytes = result?.second
            }
        }
    }
    val partTablePicker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        scope.launch(Dispatchers.IO) {
            val result = readUri(context, uri)
            withContext(Dispatchers.Main) {
                partTableFileName = result?.first
                partTableBytes = result?.second
            }
        }
    }
    val appPicker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        scope.launch(Dispatchers.IO) {
            val result = readUri(context, uri)
            withContext(Dispatchers.Main) {
                appFileName = result?.first
                appBytes = result?.second
            }
        }
    }

    // Load releases on first composition or retry
    LaunchedEffect(loadTrigger) {
        if (!useGitHub) return@LaunchedEffect
        releasesLoading = true
        releasesError = false
        val result = withContext(Dispatchers.IO) { GithubReleases.listFirmware(10) }
        releasesLoading = false
        if (result.isEmpty()) {
            releasesError = true
        } else {
            releases = result
            if (selectedRelease == null) selectedRelease = result.firstOrNull()
        }
    }

    // When source switches to GitHub and we haven't loaded yet, trigger a load
    LaunchedEffect(useGitHub) {
        if (useGitHub && releases.isEmpty() && !releasesLoading) {
            loadTrigger++
        }
    }

    // Determine the selected asset info for helper text
    val selectedAsset = if (useGitHub) {
        selectedRelease?.let { fw -> if (useFullImage) fw.full() else fw.flash() }
    } else null

    // Can we flash?
    val canFlash = !busy && when {
        useGitHub -> selectedRelease != null && selectedAsset != null
        localFormatMerged -> mergedBytes != null
        else -> bootBytes != null && partTableBytes != null && appBytes != null
    }

    val flashLabel = when {
        useGitHub -> "Download & flash over USB"
        localFormatMerged -> "Flash merged image over USB"
        else -> "Flash partitions over USB"
    }

    SectionCard(title = "USB firmware flash (recovery)") {
        Text(
            "Writes firmware over USB. Recovers a bricked or bootlooping radio with no PC.",
            color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp,
        )
        Text(
            "Bootloader mode: hold BOOT, press and release RESET, then release BOOT.",
            color = MaterialTheme.colorScheme.tertiary, fontSize = 12.sp,
        )

        // Source toggle
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(
                selected = useGitHub,
                onClick = { useGitHub = true },
                label = { Text("GitHub releases") },
            )
            FilterChip(
                selected = !useGitHub,
                onClick = { useGitHub = false },
                label = { Text("Local file") },
            )
        }

        if (useGitHub) {
            // GitHub panel
            when {
                releasesLoading -> {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        CircularProgressIndicator(modifier = Modifier.height(20.dp).padding(0.dp))
                        Text("Loading releases…", fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
                releasesError -> {
                    Text("Failed to load releases.", fontSize = 13.sp,
                        color = MaterialTheme.colorScheme.error)
                    OutlinedButton(onClick = { releasesError = false; loadTrigger++ }) {
                        Text("Retry")
                    }
                }
                else -> {
                    // Release picker dropdown
                    ExposedDropdownMenuBox(
                        expanded = dropdownExpanded,
                        onExpandedChange = { dropdownExpanded = it },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        OutlinedTextField(
                            value = selectedRelease?.displayName ?: "Select release…",
                            onValueChange = {},
                            readOnly = true,
                            label = { Text("Firmware version") },
                            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = dropdownExpanded) },
                            modifier = Modifier
                                .fillMaxWidth()
                                .menuAnchor(MenuAnchorType.PrimaryNotEditable, true),
                        )
                        ExposedDropdownMenu(
                            expanded = dropdownExpanded,
                            onDismissRequest = { dropdownExpanded = false },
                        ) {
                            releases.forEach { fw ->
                                DropdownMenuItem(
                                    text = { Text(fw.displayName) },
                                    onClick = {
                                        selectedRelease = fw
                                        dropdownExpanded = false
                                    },
                                )
                            }
                        }
                    }

                    // Image type toggle
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilterChip(
                            selected = useFullImage,
                            onClick = { useFullImage = true },
                            label = { Text("Full 8 MB") },
                        )
                        FilterChip(
                            selected = !useFullImage,
                            onClick = { useFullImage = false },
                            label = { Text("Flash merged") },
                        )
                    }

                    // Asset helper text
                    if (selectedAsset != null) {
                        Text(
                            "${selectedAsset.name}  (${selectedAsset.size / 1024} KB)",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    } else if (selectedRelease != null) {
                        Text(
                            "Not available for this release",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.error,
                        )
                    }
                }
            }
        } else {
            // Local file panel — format toggle
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = localFormatMerged,
                    onClick = { localFormatMerged = true },
                    label = { Text("Merged") },
                )
                FilterChip(
                    selected = !localFormatMerged,
                    onClick = { localFormatMerged = false },
                    label = { Text("Partitions") },
                )
            }

            if (localFormatMerged) {
                FilePickerRow(
                    label = "Merged .bin",
                    fileName = mergedFileName,
                    onPick = { mergedPicker.launch(arrayOf("application/octet-stream", "*/*")) },
                )
            } else {
                Text(
                    "Select the three partition files produced by the build system. " +
                    "They will be written at fixed offsets: bootloader at 0x00000, " +
                    "partition table at 0x08000, app binary at 0x10000.",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                FilePickerRow(
                    label = "Bootloader (0x00000)",
                    fileName = bootFileName,
                    onPick = { bootPicker.launch(arrayOf("application/octet-stream", "*/*")) },
                )
                FilePickerRow(
                    label = "Partition table (0x08000)",
                    fileName = partTableFileName,
                    onPick = { partTablePicker.launch(arrayOf("application/octet-stream", "*/*")) },
                )
                FilePickerRow(
                    label = "App binary (0x10000)",
                    fileName = appFileName,
                    onPick = { appPicker.launch(arrayOf("application/octet-stream", "*/*")) },
                )
            }
        }

        // Status + progress
        Text(statusText, fontSize = 13.sp)
        if (busy) LinearProgressIndicator(progress = { pct / 100f }, modifier = Modifier.fillMaxWidth())

        // Flash button
        Button(
            enabled = canFlash,
            modifier = Modifier.fillMaxWidth(),
            onClick = {
                busy = true; pct = 0; statusText = "Preparing…"
                scope.launch(Dispatchers.IO) {
                    val cb = object : EspFlasher.Progress {
                        override fun status(message: String) { post { statusText = message } }
                        override fun percent(value: Int) { post { pct = value } }
                    }

                    if (useGitHub) {
                        val fw = selectedRelease
                        val asset = fw?.let { if (useFullImage) it.full() else it.flash() }
                        if (asset == null) {
                            post { statusText = "No firmware asset available"; busy = false }
                            return@launch
                        }
                        post { statusText = "Downloading ${asset.name}…" }
                        val bytes = GithubReleases.download(asset.url) { p -> post { pct = p } }
                        if (bytes == null) {
                            post { statusText = "Download failed"; busy = false }
                            return@launch
                        }
                        val port = Controllers.usb.takePortForFlashing()
                        if (port == null) {
                            post { statusText = "Connect USB & enter bootloader mode first"; busy = false }
                            return@launch
                        }
                        val ok = EspFlasher(port).flash(0x0, bytes, cb)
                        Controllers.usb.resumeAfterFlash()
                        post { statusText = if (ok) "Flashed — radio rebooting" else statusText; busy = false }
                    } else if (localFormatMerged) {
                        val bytes = mergedBytes
                        if (bytes == null) {
                            post { statusText = "No file selected"; busy = false }
                            return@launch
                        }
                        val port = Controllers.usb.takePortForFlashing()
                        if (port == null) {
                            post { statusText = "Connect USB & enter bootloader mode first"; busy = false }
                            return@launch
                        }
                        val ok = EspFlasher(port).flash(0x0, bytes, cb)
                        Controllers.usb.resumeAfterFlash()
                        post { statusText = if (ok) "Flashed — radio rebooting" else statusText; busy = false }
                    } else {
                        val boot = bootBytes
                        val partTable = partTableBytes
                        val app = appBytes
                        if (boot == null || partTable == null || app == null) {
                            post { statusText = "Select all three partition files first"; busy = false }
                            return@launch
                        }
                        val port = Controllers.usb.takePortForFlashing()
                        if (port == null) {
                            post { statusText = "Connect USB & enter bootloader mode first"; busy = false }
                            return@launch
                        }
                        val ok = EspFlasher(port).flashParts(
                            listOf(0x0 to boot, 0x8000 to partTable, 0x10000 to app),
                            cb,
                        )
                        Controllers.usb.resumeAfterFlash()
                        post { statusText = if (ok) "Flashed — radio rebooting" else statusText; busy = false }
                    }
                }
            },
        ) { Text(flashLabel) }
    }
}

@Composable
private fun RecoveryOtaCard() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val shizukuGranted by ShizukuManager.granted.collectAsStateWithLifecycle()

    // Source toggle
    var useGitHub by remember { mutableStateOf(true) }
    var otaFileName by remember { mutableStateOf<String?>(null) }
    var otaBytes by remember { mutableStateOf<ByteArray?>(null) }

    var busy by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("Idle") }
    var pct by remember { mutableIntStateOf(0) }

    val otaPicker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        scope.launch(Dispatchers.IO) {
            val result = readUri(context, uri)
            withContext(Dispatchers.Main) {
                otaFileName = result?.first
                otaBytes = result?.second
            }
        }
    }

    val canUpload = !busy && (useGitHub || otaBytes != null)

    SectionCard(title = "Recovery AP firmware update") {
        Text(
            "Auto-joins the radio's \"ATS-Mini Recovery\" Wi-Fi (hold the encoder at power-on) and uploads firmware over the air — one tap with Shizuku.",
            color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp,
        )

        // Source toggle
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(
                selected = useGitHub,
                onClick = { useGitHub = true },
                label = { Text("GitHub latest") },
            )
            FilterChip(
                selected = !useGitHub,
                onClick = { useGitHub = false },
                label = { Text("Local file") },
            )
        }

        if (!useGitHub) {
            FilePickerRow(
                label = "OTA .bin",
                fileName = otaFileName,
                onPick = { otaPicker.launch(arrayOf("application/octet-stream", "*/*")) },
            )
        }

        Text(statusText, fontSize = 13.sp)
        if (busy) LinearProgressIndicator(progress = { pct / 100f }, modifier = Modifier.fillMaxWidth())
        Button(
            enabled = canUpload,
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

                    val bytes: ByteArray?
                    if (useGitHub) {
                        val fw = GithubReleases.latestFirmware()
                        val asset = fw?.ota()
                        if (asset == null) {
                            post { statusText = "No OTA asset found"; busy = false }
                            return@launch
                        }
                        post { statusText = "Downloading ${asset.name}…" }
                        bytes = GithubReleases.download(asset.url) { p -> post { pct = p } }
                    } else {
                        bytes = otaBytes
                    }

                    if (bytes == null) {
                        post { statusText = "Download failed"; busy = false }
                        return@launch
                    }
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

@Composable
private fun FilePickerRow(label: String, fileName: String?, onPick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(label, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(
                fileName ?: "Not selected",
                fontSize = 12.sp,
                color = if (fileName != null) MaterialTheme.colorScheme.onSurface
                        else MaterialTheme.colorScheme.outline,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        OutlinedButton(onClick = onPick) { Text("Pick") }
    }
}
