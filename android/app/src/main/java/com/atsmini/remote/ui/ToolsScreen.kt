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
import androidx.compose.material.icons.filled.Warning

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

/** Image kind selectable for USB flashing. Offset is where the ROM writes it. */
private enum class FlashKind(val label: String, val offset: Int, val desc: String) {
    FULL("Full", 0x0, "Erases everything including presets & saved settings. Clean install / unbrick."),
    FIRMWARE("Firmware", 0x0, "App + bootloader; keeps presets & settings (LittleFS user data)."),
    RECOVERY("Recovery", 0x10000, "Re-flashes only the recovery factory image at 0x10000."),
}

/** Which image kind a file/asset name corresponds to (used for cache + local picks). */
private fun inferKind(name: String): FlashKind = when {
    name.contains("recovery", ignoreCase = true) -> FlashKind.RECOVERY
    name.contains("full", ignoreCase = true) -> FlashKind.FULL
    else -> FlashKind.FIRMWARE
}

/**
 * Returns a human-readable warning if [imageName] doesn't match [selectedKind], or null
 * if the combination looks safe.  Prevents cross-partition flashing accidents.
 */
private fun imageKindWarning(imageName: String, selectedKind: FlashKind): String? {
    val inferred = inferKind(imageName)
    return when {
        inferred == FlashKind.RECOVERY && selectedKind != FlashKind.RECOVERY ->
            "⚠ '$imageName' looks like a recovery image but '${selectedKind.label}' is selected. " +
            "Recovery images must be flashed at 0x10000 — switch to Recovery kind."
        inferred != FlashKind.RECOVERY && selectedKind == FlashKind.RECOVERY ->
            "⚠ '$imageName' doesn't look like a recovery image. " +
            "Recovery slot expects an '*-ospi-recovery.bin' file."
        inferred == FlashKind.FULL && selectedKind == FlashKind.FIRMWARE ->
            "ℹ '$imageName' is a full 8 MB image (erases LittleFS). " +
            "To preserve settings use an '*-ospi-flash.bin' instead."
        else -> null
    }
}

/** Returns a warning if [imageName] is unsuitable for Wi-Fi OTA (not an OTA binary). */
private fun wifiOtaImageWarning(imageName: String): String? =
    if (!imageName.contains("ota", ignoreCase = true) &&
        !imageName.contains("update", ignoreCase = true))
        "⚠ '$imageName' doesn't look like an OTA image. Wi-Fi OTA requires an '*-ospi-ota.bin'. " +
        "Flashing a full/flash/recovery image via OTA will brick the device."
    else null

/** Source of the bytes to flash/upload. */
private enum class FwSource { GITHUB, CACHED, LOCAL }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun UsbFlashPanel(busy: Boolean, ctl: OpControls) {
    val context = LocalContext.current

    var kind by remember { mutableStateOf(FlashKind.FIRMWARE) }
    var source by remember { mutableStateOf(FwSource.GITHUB) }

    var releases by remember { mutableStateOf<List<GithubReleases.Firmware>>(emptyList()) }
    var releasesLoading by remember { mutableStateOf(false) }
    var releasesError by remember { mutableStateOf(false) }
    var selectedRelease by remember { mutableStateOf<GithubReleases.Firmware?>(null) }
    var dropdownExpanded by remember { mutableStateOf(false) }
    var loadTrigger by remember { mutableIntStateOf(0) }

    var cached by remember { mutableStateOf(com.atsmini.remote.net.FirmwareCache.list()) }
    var selectedCache by remember { mutableStateOf<com.atsmini.remote.net.FirmwareCache.Entry?>(null) }
    var cacheExpanded by remember { mutableStateOf(false) }

    var fileName by remember { mutableStateOf<String?>(null) }
    var fileBytes by remember { mutableStateOf<ByteArray?>(null) }

    val picker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        ctl.scope.launch(Dispatchers.IO) {
            val r = readUri(context, uri)
            post { fileName = r?.first; fileBytes = r?.second; r?.first?.let { kind = inferKind(it) } }
        }
    }

    LaunchedEffect(loadTrigger, source) {
        if (source != FwSource.GITHUB || releases.isNotEmpty()) return@LaunchedEffect
        releasesLoading = true; releasesError = false
        val result = kotlinx.coroutines.withContext(Dispatchers.IO) { GithubReleases.listFirmware(15) }
        releasesLoading = false
        if (result.isEmpty()) releasesError = true
        else { releases = result; if (selectedRelease == null) selectedRelease = result.firstOrNull() }
    }
    LaunchedEffect(source) { if (source == FwSource.CACHED) cached = com.atsmini.remote.net.FirmwareCache.list() }

    val selectedAsset = if (source == FwSource.GITHUB) selectedRelease?.let {
        when (kind) {
            FlashKind.FULL -> it.full()
            FlashKind.FIRMWARE -> it.flash()
            FlashKind.RECOVERY -> it.recovery()
        }
    } else null

    val canFlash = !busy && when (source) {
        FwSource.GITHUB -> selectedAsset != null
        FwSource.CACHED -> selectedCache != null
        FwSource.LOCAL -> fileBytes != null
    }

    SectionCard(title = "Flash over USB cable") {
        Text("Recovers or updates the radio with no PC. The app auto-resets the radio " +
            "into download mode over USB from normal, recovery, or bootloader mode — " +
            "if that fails, hold BOOT, tap RESET, release BOOT and retry.",
            fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

        Text("Image", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FlashKind.entries.forEach { k ->
                FilterChip(selected = kind == k, onClick = { kind = k }, label = { Text(k.label) })
            }
        }
        Text(kind.desc, fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            SegmentedButton(selected = source == FwSource.GITHUB, onClick = { source = FwSource.GITHUB },
                shape = SegmentedButtonDefaults.itemShape(0, 3)) { Text("GitHub") }
            SegmentedButton(selected = source == FwSource.CACHED, onClick = { source = FwSource.CACHED },
                shape = SegmentedButtonDefaults.itemShape(1, 3)) { Text("Cached") }
            SegmentedButton(selected = source == FwSource.LOCAL, onClick = { source = FwSource.LOCAL },
                shape = SegmentedButtonDefaults.itemShape(2, 3)) { Text("Local file") }
        }

        when (source) {
            FwSource.GITHUB -> when {
                releasesLoading -> Row(verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    CircularProgressIndicator(Modifier.height(20.dp))
                    Text("Loading releases…", fontSize = 13.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                releasesError -> {
                    Text("Failed to load releases. Try the Cached tab if offline.",
                        fontSize = 13.sp, color = MaterialTheme.colorScheme.error)
                    OutlinedButton(onClick = { releasesError = false; loadTrigger++ }) { Text("Retry") }
                }
                else -> {
                    ReleaseDropdown(releases, selectedRelease, dropdownExpanded,
                        onExpand = { dropdownExpanded = it }, onSelect = { selectedRelease = it; dropdownExpanded = false })
                    if (selectedAsset != null) Text("${selectedAsset.name}  (${selectedAsset.size / 1024} KB)" +
                        if (com.atsmini.remote.net.FirmwareCache.has(selectedAsset.name)) "  • cached" else "",
                        fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant,
                        maxLines = 1, overflow = TextOverflow.Ellipsis)
                    else if (selectedRelease != null) Text("No ${kind.label} image for this release",
                        fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
                }
            }
            FwSource.CACHED -> CacheDropdown(cached, selectedCache, cacheExpanded,
                onExpand = { cacheExpanded = it },
                onSelect = { selectedCache = it; cacheExpanded = false; kind = inferKind(it.name) })
            FwSource.LOCAL -> FilePickerRow(label = "Firmware .bin (${kind.label})", fileName = fileName,
                onPick = { picker.launch(arrayOf("application/octet-stream", "*/*")) })
        }

        // Validation warning: check selected image matches the selected kind
        val validationWarning = when (source) {
            FwSource.GITHUB -> selectedAsset?.name?.let { imageKindWarning(it, kind) }
            FwSource.CACHED -> selectedCache?.name?.let { imageKindWarning(it, kind) }
            FwSource.LOCAL -> fileName?.let { imageKindWarning(it, kind) }
        }
        if (validationWarning != null) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Filled.Warning, contentDescription = null,
                    tint = MaterialTheme.colorScheme.error)
                Text(validationWarning, fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
            }
        }

        Button(enabled = canFlash && validationWarning == null, modifier = Modifier.fillMaxWidth(), onClick = {
            ctl.setBusy(true); ctl.setStatus("Preparing…"); ctl.setPct(0)
            ctl.scope.launch(Dispatchers.IO) {
                try {
                    val bytes = when (source) {
                        FwSource.GITHUB -> {
                            val asset = selectedAsset ?: run { ctl.setStatus("No asset available"); return@launch }
                            ctl.setStatus("Fetching ${asset.name}…")
                            GithubReleases.downloadCached(asset) { ctl.setPct(it) }
                                ?: run { ctl.setStatus("Download failed — try Cached if offline"); return@launch }
                        }
                        FwSource.CACHED -> {
                            val e = selectedCache ?: run { ctl.setStatus("No cached image selected"); return@launch }
                            com.atsmini.remote.net.FirmwareCache.read(e.name)
                                ?: run { ctl.setStatus("Cached file unreadable"); return@launch }
                        }
                        FwSource.LOCAL -> fileBytes ?: run { ctl.setStatus("No file selected"); return@launch }
                    }

                    // ── Step 1: try direct ROM sync (device already in bootloader mode) ──
                    val initialPort = Controllers.usb.takePortForFlashing()
                        ?: run { ctl.setStatus("Connect the USB cable first"); return@launch }

                    if (EspFlasher(initialPort).flash(kind.offset, bytes, ctl.flashCb, skipAutoReset = true)) {
                        Controllers.usb.resumeAfterFlash()
                        return@launch
                    }

                    // ── Step 2: trigger download mode, reconnect, retry ──────────────────
                    // Send soft reboot_dl command if radio firmware is active on USB.
                    val connectedViaUsb = RadioRepository.status.value.transport == Transport.USB
                    if (connectedViaUsb) {
                        ctl.setStatus("Sending download-mode command to firmware…")
                        Controllers.usb.writeLine("{\"cmd\":\"reboot_dl\"}")
                        delay(300) // let firmware process the command before closing
                    } else {
                        ctl.setStatus("Triggering download mode via USB reset…")
                    }
                    Controllers.usb.closeForReset()

                    ctl.setStatus("Waiting for device to reboot into download mode…")
                    delay(if (connectedViaUsb) 2000L else 800L)

                    val newPort = Controllers.usb.waitAndReopenForFlashing(8000L)
                        ?: run {
                            ctl.setStatus(
                                "Device didn't reconnect after reset.\n\n" +
                                "If a USB permission dialog appeared, grant it and tap Flash again.\n\n" +
                                "Manual entry: Hold BOOT → tap RESET → release BOOT → tap Flash."
                            )
                            return@launch
                        }

                    val ok = EspFlasher(newPort).flash(kind.offset, bytes, ctl.flashCb, skipAutoReset = true)
                    Controllers.usb.resumeAfterFlash()
                    if (!ok) ctl.setStatus(
                        "Flash failed.\n\nManual bootloader entry:\n" +
                        "Hold BOOT → tap RESET → release BOOT → tap Flash."
                    )
                } finally { ctl.setBusy(false) }
            }
        }) { Text("Flash ${kind.label} over USB") }
    }
}

/** Adhoc = radio's own recovery AP (no internet). Network = both on the same Wi-Fi. */
private enum class OtaTarget { ADHOC, NETWORK }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun WifiOtaPanel(busy: Boolean, ctl: OpControls) {
    val context = LocalContext.current
    val shizukuGranted by ShizukuManager.granted.collectAsStateWithLifecycle()
    val status by RadioRepository.status.collectAsStateWithLifecycle()

    var target by remember { mutableStateOf(OtaTarget.ADHOC) }
    var host by remember { mutableStateOf("") }
    // Prefill the network host from the radio's reported IP when we learn it.
    LaunchedEffect(status.wifiIP) { if (host.isBlank() && status.wifiIP.isNotBlank()) host = status.wifiIP }

    var source by remember { mutableStateOf(FwSource.GITHUB) }
    var cached by remember { mutableStateOf(com.atsmini.remote.net.FirmwareCache.list()) }
    var selectedCache by remember { mutableStateOf<com.atsmini.remote.net.FirmwareCache.Entry?>(null) }
    var cacheExpanded by remember { mutableStateOf(false) }
    var fileName by remember { mutableStateOf<String?>(null) }
    var fileBytes by remember { mutableStateOf<ByteArray?>(null) }

    val picker = rememberLauncherForActivityResult(OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        ctl.scope.launch(Dispatchers.IO) {
            val r = readUri(context, uri)
            post { fileName = r?.first; fileBytes = r?.second }
        }
    }
    LaunchedEffect(source) { if (source == FwSource.CACHED) cached = com.atsmini.remote.net.FirmwareCache.list() }

    val adhocNoInternet = target == OtaTarget.ADHOC && source == FwSource.GITHUB
    val networkHost = if (target == OtaTarget.NETWORK) host.trim().ifEmpty { "atsmini.local" } else RecoveryOta.RECOVERY_HOST
    val canUpload = !busy && when (source) {
        FwSource.GITHUB -> true
        FwSource.CACHED -> selectedCache != null
        FwSource.LOCAL -> fileBytes != null
    } && (target == OtaTarget.ADHOC || networkHost.isNotBlank())

    SectionCard(title = "Update over Wi-Fi (no cable)") {
        Text("Boot the radio into recovery (hold the encoder at power-on). Push an OTA " +
            "image to it over Wi-Fi.",
            fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

        Text("Connection", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            SegmentedButton(selected = target == OtaTarget.ADHOC, onClick = { target = OtaTarget.ADHOC },
                shape = SegmentedButtonDefaults.itemShape(0, 2)) { Text("Adhoc AP") }
            SegmentedButton(selected = target == OtaTarget.NETWORK, onClick = { target = OtaTarget.NETWORK },
                shape = SegmentedButtonDefaults.itemShape(1, 2)) { Text("Same network") }
        }
        if (target == OtaTarget.ADHOC) {
            Text("Radio hosts the \"${RecoveryOta.RECOVERY_SSID}\" Wi-Fi. " +
                if (shizukuGranted) "Shizuku joins it for you." else "Join it in Wi-Fi settings first.",
                fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        } else {
            OutlinedTextField(value = host, onValueChange = { host = it }, singleLine = true,
                label = { Text("Radio IP / hostname") },
                placeholder = { Text("atsmini.local") }, modifier = Modifier.fillMaxWidth())
            Text("Both the radio (recovery → Network → join Wi-Fi) and this device must be " +
                "on the same network.", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }

        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            SegmentedButton(selected = source == FwSource.GITHUB, onClick = { source = FwSource.GITHUB },
                shape = SegmentedButtonDefaults.itemShape(0, 3)) { Text("GitHub") }
            SegmentedButton(selected = source == FwSource.CACHED, onClick = { source = FwSource.CACHED },
                shape = SegmentedButtonDefaults.itemShape(1, 3)) { Text("Cached") }
            SegmentedButton(selected = source == FwSource.LOCAL, onClick = { source = FwSource.LOCAL },
                shape = SegmentedButtonDefaults.itemShape(2, 3)) { Text("Local file") }
        }
        when (source) {
            FwSource.GITHUB -> if (adhocNoInternet) Text(
                "On the adhoc AP this device has no internet. Pre-download on a normal " +
                "network, or use a Cached / Local image.",
                fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
            FwSource.CACHED -> CacheDropdown(cached, selectedCache, cacheExpanded,
                onExpand = { cacheExpanded = it }, onSelect = { selectedCache = it; cacheExpanded = false })
            FwSource.LOCAL -> FilePickerRow(label = "OTA .bin", fileName = fileName,
                onPick = { picker.launch(arrayOf("application/octet-stream", "*/*")) })
        }

        // Warn if user selected a non-OTA image for Wi-Fi OTA upload
        val otaWarn = when (source) {
            FwSource.CACHED -> selectedCache?.name?.let { wifiOtaImageWarning(it) }
            FwSource.LOCAL -> fileName?.let { wifiOtaImageWarning(it) }
            else -> null
        }
        if (otaWarn != null) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Filled.Warning, contentDescription = null,
                    tint = MaterialTheme.colorScheme.error)
                Text(otaWarn, fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
            }
        }

        Button(enabled = canUpload && otaWarn == null, modifier = Modifier.fillMaxWidth(), onClick = {
            ctl.setBusy(true); ctl.setPct(0)
            ctl.scope.launch(Dispatchers.IO) {
                try {
                    if (target == OtaTarget.ADHOC) {
                        if (shizukuGranted) {
                            ctl.setStatus("Joining ${RecoveryOta.RECOVERY_SSID}…")
                            ShizukuManager.connectWifi(RecoveryOta.RECOVERY_SSID, RecoveryOta.RECOVERY_PASS)
                            delay(8000)
                        } else {
                            ctl.setStatus("Join '${RecoveryOta.RECOVERY_SSID}' Wi-Fi, then wait…")
                            delay(4000)
                        }
                    }
                    val bytes = when (source) {
                        FwSource.GITHUB -> {
                            val asset = GithubReleases.latestFirmware()?.ota()
                                ?: run { ctl.setStatus("No OTA asset found — use Cached/Local"); return@launch }
                            ctl.setStatus("Fetching ${asset.name}…")
                            GithubReleases.downloadCached(asset) { ctl.setPct(it) }
                        }
                        FwSource.CACHED -> selectedCache?.let { com.atsmini.remote.net.FirmwareCache.read(it.name) }
                        FwSource.LOCAL -> fileBytes
                    }
                    if (bytes == null) { ctl.setStatus("Image unavailable"); return@launch }
                    ctl.setStatus("Uploading to $networkHost…")
                    RecoveryOta.upload(networkHost, bytes, ctl.otaCb)
                } finally { ctl.setBusy(false) }
            }
        }) { Text(if (target == OtaTarget.ADHOC && shizukuGranted) "Auto-join & upload OTA" else "Upload OTA") }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ReleaseDropdown(
    releases: List<GithubReleases.Firmware>,
    selected: GithubReleases.Firmware?,
    expanded: Boolean,
    onExpand: (Boolean) -> Unit,
    onSelect: (GithubReleases.Firmware) -> Unit,
) {
    ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = onExpand, modifier = Modifier.fillMaxWidth()) {
        OutlinedTextField(
            value = selected?.displayName ?: "Select release…",
            onValueChange = {}, readOnly = true,
            label = { Text("Firmware version") },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            modifier = Modifier.fillMaxWidth().menuAnchor(MenuAnchorType.PrimaryNotEditable, true),
        )
        ExposedDropdownMenu(expanded = expanded, onDismissRequest = { onExpand(false) }) {
            releases.forEach { fw ->
                DropdownMenuItem(text = { Text(fw.displayName) }, onClick = { onSelect(fw) })
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CacheDropdown(
    cached: List<com.atsmini.remote.net.FirmwareCache.Entry>,
    selected: com.atsmini.remote.net.FirmwareCache.Entry?,
    expanded: Boolean,
    onExpand: (Boolean) -> Unit,
    onSelect: (com.atsmini.remote.net.FirmwareCache.Entry) -> Unit,
) {
    if (cached.isEmpty()) {
        Text("No cached images yet. Download one over GitHub first; the last " +
            "${com.atsmini.remote.net.FirmwareCache.MAX_ENTRIES} are kept for offline use.",
            fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        return
    }
    ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = onExpand, modifier = Modifier.fillMaxWidth()) {
        OutlinedTextField(
            value = selected?.let { "${it.name} (${it.size / 1024} KB)" } ?: "Select cached image…",
            onValueChange = {}, readOnly = true,
            label = { Text("Cached firmware (${cached.size}/${com.atsmini.remote.net.FirmwareCache.MAX_ENTRIES})") },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            modifier = Modifier.fillMaxWidth().menuAnchor(MenuAnchorType.PrimaryNotEditable, true),
        )
        ExposedDropdownMenu(expanded = expanded, onDismissRequest = { onExpand(false) }) {
            cached.forEach { e ->
                DropdownMenuItem(
                    text = { Text("${e.name}  (${e.size / 1024} KB)", maxLines = 1, overflow = TextOverflow.Ellipsis) },
                    onClick = { onSelect(e) })
            }
        }
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
