package com.atsmini.remote.usb

import com.hoho.android.usbserial.driver.UsbSerialPort

/**
 * Serial firmware OTA over the running firmware's USB JSON link.
 *
 * Unlike [EspFlasher] (which drives the ROM bootloader and needs the device in
 * download mode), this talks to the *running* firmware: it sends
 * `{"cmd":"ota_begin","size":N}` (or `"rec_begin"` to target the recovery /
 * factory partition), streams the raw image, and the firmware writes it via
 * esp_ota / Update and reboots into the new image. No reset, no BOOT button,
 * and the device reliably reboots itself when done.
 *
 * Mirrors the proven serial-OTA path in the recovery firmware.
 */
class SerialOta(private val port: UsbSerialPort) {

    interface Progress {
        fun status(message: String)
        fun percent(value: Int)
    }

    private val rx = StringBuilder()

    /**
     * Flash [image] over the serial link. When [recovery] is true the image is
     * written to the factory/recovery partition (device must be running normal
     * firmware, not recovery). Returns true on a confirmed successful flash.
     */
    fun flash(image: ByteArray, recovery: Boolean, progress: Progress): Boolean {
        drain()
        // CRC32 (zlib/IEEE) of the image, sent as a SIGNED 32-bit int so the
        // firmware's atol-based parser round-trips it (the unsigned form would
        // overflow). The firmware verifies it before committing the flash.
        val crc = java.util.zip.CRC32().apply { update(image) }.value.toInt()
        val cmd = if (recovery) "rec_begin" else "ota_begin"
        val beginCmd = "{\"cmd\":\"$cmd\",\"size\":${image.size},\"crc\":$crc}"

        // Quiet the live status stream first so its packets don't drown the reply.
        writeLine("{\"cmd\":\"sub\",\"ms\":0}")
        Thread.sleep(150)
        drain()

        // ── Pre-flight: confirm two-way USB comms are actually alive ──────────
        // This is the key diagnostic. esptool talks to the ROM bootloader (a
        // hardware download mode); we instead talk to the *running* firmware over
        // USB serial, so the link must be live AND the firmware new enough to
        // speak the protocol. If we can't even get a "pong" back there's no point
        // streaming a multi-MB image — and we can tell the user precisely what's
        // wrong (dead link vs. too-old firmware) instead of a vague stall.
        if (!preflight(progress)) return false

        var begin: String? = null
        for (attempt in 1..6) {
            progress.status("Requesting OTA slot from firmware… (try $attempt)")
            if (!writeLine(beginCmd)) {
                progress.status("USB write failed — is the cable connected?")
                return false
            }
            begin = waitForResult(3000, progress)
            if (begin != null) break
            drain()
        }
        when {
            begin == null -> {
                progress.status(
                    "Firmware answered the ping but not the OTA request.\n\n" +
                    "This usually means the installed firmware predates live OTA " +
                    "support (needs v2.66+). Update once via the Bootloader method " +
                    "(hold BOOT, tap RESET, release BOOT) or Wi-Fi OTA, then Live " +
                    "flashing will work."
                )
                return false
            }
            !begin.contains("\"ok\":true") -> {
                progress.status("Device rejected OTA: $begin")
                return false
            }
        }

        progress.status("Streaming firmware over USB…")
        var sent = 0
        val block = 1024
        while (sent < image.size) {
            val n = minOf(block, image.size - sent)
            val ok = runCatching { port.write(image.copyOfRange(sent, sent + n), 3000); true }
                .getOrDefault(false)
            if (!ok) { progress.status("USB write failed at $sent / ${image.size} bytes"); return false }
            sent += n
            progress.percent(sent * 100 / image.size)
        }

        progress.status("Verifying & finalizing…")
        val done = waitForResult(20000, progress) ?: run {
            progress.status(
                "No completion response.\n\n" +
                "The device may still be flashing/rebooting — give it a few seconds, " +
                "then reconnect. If it doesn't come back, reflash via USB Full (bootloader mode)."
            )
            return false
        }
        val ok = done.contains("\"ok\":true")
        progress.status(
            when {
                !ok -> "Flash failed: $done"
                // Recovery self-migration: the radio reboots and installs to the
                // factory partition on its own, showing progress on its screen.
                done.contains("\"stage\":1") ->
                    "Recovery received & verified — the radio is rebooting to install it. " +
                    "Watch the radio screen; it'll reboot a couple of times. Don't unplug."
                else -> "Done — radio rebooting into new firmware"
            }
        )
        return ok
    }

    /**
     * Probe the link with `{"cmd":"ping"}` and wait for `{"t":"pong"}`. Tries a
     * few times (the first byte after the console-reader handover can be lost).
     * Returns true once the firmware answers; on failure sets a precise status
     * and returns false so the caller aborts before streaming the image.
     */
    private fun preflight(progress: Progress): Boolean {
        for (attempt in 1..6) {
            progress.status("Checking USB link to the radio… (try $attempt)")
            if (!writeLine("{\"cmd\":\"ping\"}")) {
                progress.status("USB write failed — is the cable connected and the radio on?")
                return false
            }
            val reply = waitForPong(800)
            if (reply) return true
            drain()
        }
        progress.status(
            "No response from the radio over USB.\n\n" +
            "The serial link isn't answering, so live flashing can't run. Try:\n" +
            "• Tap Device → Disconnect, then Connect, and Flash again.\n" +
            "• Re-seat the USB cable (use a data cable, not charge-only).\n\n" +
            "Reliable fallback (works from any state, like esptool): switch Method " +
            "to Bootloader — hold BOOT, tap RESET, release BOOT — then Flash."
        )
        return false
    }

    /** Read lines until a `{"t":"pong"}` arrives or timeout. */
    private fun waitForPong(timeoutMs: Int): Boolean {
        val deadline = System.currentTimeMillis() + timeoutMs
        val buf = ByteArray(512)
        while (System.currentTimeMillis() < deadline) {
            val n = runCatching { port.read(buf, 200) }.getOrDefault(0)
            if (n > 0) {
                rx.append(String(buf, 0, n))
                var idx = rx.indexOf("\n")
                while (idx >= 0) {
                    val line = rx.substring(0, idx)
                    rx.delete(0, idx + 1)
                    if (line.contains("pong")) { rx.setLength(0); return true }
                    idx = rx.indexOf("\n")
                }
            }
        }
        return false
    }

    private fun writeLine(s: String): Boolean =
        runCatching { port.write((s + "\n").toByteArray(), 2000); true }.getOrDefault(false)

    /**
     * Read serial lines until an OTA result line (begin ack / completion / error)
     * arrives, or until timeout. Recognises both firmwares' reply formats:
     *   main fw:   {"t":"ota","ok":true,...} / {"t":"ota","progress":P,"total":T}
     *   recovery:  {"ok":true,...}           / {"progress":P,"total":T}
     * A line containing "ok" is a result; one containing "progress" updates the
     * device-confirmed percent. Other JSON (status, etc.) is ignored.
     */
    private fun waitForResult(timeoutMs: Int, progress: Progress): String? {
        val deadline = System.currentTimeMillis() + timeoutMs
        val buf = ByteArray(512)
        while (System.currentTimeMillis() < deadline) {
            val n = runCatching { port.read(buf, 200) }.getOrDefault(0)
            if (n > 0) {
                rx.append(String(buf, 0, n))
                var idx = rx.indexOf("\n")
                while (idx >= 0) {
                    val line = rx.substring(0, idx).trim()
                    rx.delete(0, idx + 1)
                    when {
                        line.contains("\"ok\"") -> return line          // begin ack, done, or error
                        line.contains("\"progress\"") -> parseProgress(line)?.let { progress.percent(it) }
                    }
                    idx = rx.indexOf("\n")
                }
            }
        }
        return null
    }

    /** Extract a 0–100 percent from a `{"t":"ota","progress":P,"total":T}` line. */
    private fun parseProgress(line: String): Int? {
        val p = Regex("\"progress\":(\\d+)").find(line)?.groupValues?.get(1)?.toLongOrNull() ?: return null
        val t = Regex("\"total\":(\\d+)").find(line)?.groupValues?.get(1)?.toLongOrNull() ?: return null
        if (t <= 0) return null
        return (p * 100 / t).toInt().coerceIn(0, 100)
    }

    private fun drain() {
        val buf = ByteArray(512)
        repeat(3) { runCatching { port.read(buf, 50) } }
        rx.setLength(0)
    }
}
