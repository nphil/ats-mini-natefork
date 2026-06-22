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
        val cmd = if (recovery) "rec_begin" else "ota_begin"
        progress.status("Requesting OTA slot from firmware…")
        if (!writeLine("{\"cmd\":\"$cmd\",\"size\":${image.size}}")) {
            progress.status("USB write failed — is the cable connected?")
            return false
        }

        val begin = waitForResult(5000, progress)
        when {
            begin == null -> {
                progress.status(
                    "No response from firmware.\n\n" +
                    "Make sure the radio is powered on and running (not in the ROM " +
                    "bootloader), then tap Flash again."
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
        progress.status(if (ok) "Done — radio rebooting into new firmware" else "Flash failed: $done")
        return ok
    }

    private fun writeLine(s: String): Boolean =
        runCatching { port.write((s + "\n").toByteArray(), 2000); true }.getOrDefault(false)

    /**
     * Read serial lines until one carrying an `{"t":"ota",...,"ok":...}` result
     * (begin ack or completion) arrives, or until timeout. Progress lines
     * (`{"t":"ota","progress":...}`) update the device-confirmed percent.
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
                    if (line.contains("\"ota\"")) {
                        if (line.contains("\"ok\"")) return line   // begin ack or done
                        parseProgress(line)?.let { progress.percent(it) }
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
