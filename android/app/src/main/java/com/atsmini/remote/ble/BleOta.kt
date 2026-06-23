package com.atsmini.remote.ble

import java.io.ByteArrayOutputStream
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.zip.Deflater

/**
 * Firmware OTA over the running firmware's BLE Nordic-UART link.
 *
 * Images are compressed with raw deflate before transfer, reducing BLE transfer
 * time by ~50%. The firmware command is `ota_begin_z` / `rec_begin_z`, with
 * `size=compressed_bytes` and `osize=original_bytes`. The firmware inflates
 * on-the-fly via miniz tinfl and verifies CRC32 of the decompressed output.
 *
 * Flow control mirrors USB: the firmware advertises a block size in its begin
 * reply; we send one block (as MTU-sized writes, each confirmed via
 * write-with-response) then wait for `{"ack":N}` before the next. This keeps the
 * stream from overrunning the radio's single-packet RX buffer.
 *
 * Requires firmware ≥ v2.76 for compression support.
 */
class BleOta(private val ble: BleManager) {

    interface Progress {
        fun status(message: String)
        fun percent(value: Int)
    }

    private val lines = LinkedBlockingQueue<String>()
    private val rxAccum = StringBuilder()

    /**
     * Flash [image] over BLE. When [recovery] is true the image targets the
     * factory/recovery partition. Returns true on a confirmed successful flash
     * (firmware accepted + verified the image and is rebooting to install it).
     */
    fun flash(image: ByteArray, recovery: Boolean, progress: Progress): Boolean {
        if (!ble.isReady) {
            progress.status("BLE not connected — connect to the radio first (Settings → Scan).")
            return false
        }
        // Take over RX so we see ACK/result lines instead of the status parser.
        ble.otaRxListener = { chunk ->
            rxAccum.append(chunk)
            var idx = rxAccum.indexOf("\n")
            while (idx >= 0) {
                val line = rxAccum.substring(0, idx).trim()
                rxAccum.delete(0, idx + 1)
                if (line.isNotEmpty()) lines.offer(line)
                idx = rxAccum.indexOf("\n")
            }
        }
        try {
            return doFlash(image, recovery, progress)
        } finally {
            ble.otaRxListener = null
        }
    }

    private fun doFlash(rawImage: ByteArray, recovery: Boolean, progress: Progress): Boolean {
        // Compress the image with raw deflate (no zlib header; firmware uses tinfl).
        progress.status("Compressing image…")
        val image = compress(rawImage)
        val savings = ((rawImage.size - image.size) * 100 / rawImage.size)
        progress.status("Compressed ${rawImage.size} → ${image.size} bytes ($savings% smaller)")

        // CRC32 is over the ORIGINAL bytes: firmware decompresses then checks CRC.
        val crc = java.util.zip.CRC32().apply { update(rawImage) }.value.toInt()
        val cmd = if (recovery) "rec_begin_z" else "ota_begin_z"
        val beginCmd = "{\"cmd\":\"$cmd\",\"size\":${image.size},\"osize\":${rawImage.size},\"crc\":$crc}"

        // Quiet the live status stream so its packets don't drown the replies.
        writeLine("{\"cmd\":\"sub\",\"ms\":0}")
        Thread.sleep(200)
        lines.clear(); rxAccum.setLength(0)

        // ── Pre-flight: confirm the BLE link answers the protocol ─────────────
        if (!preflight(progress)) return false

        var begin: String? = null
        for (attempt in 1..6) {
            progress.status("Requesting OTA slot from firmware… (try $attempt)")
            if (!writeLine(beginCmd)) {
                progress.status("BLE write failed — is the radio still connected?")
                return false
            }
            begin = waitForResult(3000, progress)
            if (begin != null) break
        }
        when {
            begin == null -> {
                progress.status(
                    "Radio answered the ping but not the OTA request.\n\n" +
                    "This usually means the installed firmware predates BLE OTA " +
                    "support. Update once over USB (Live or Bootloader) or Wi-Fi, " +
                    "then BLE flashing will work."
                )
                return false
            }
            !begin.contains("\"ok\":true") -> {
                progress.status("Radio rejected OTA: $begin")
                return false
            }
        }

        val block = Regex("\"block\":(\\d+)").find(begin!!)?.groupValues?.get(1)?.toIntOrNull() ?: 0
        if (block <= 0) {
            progress.status("Radio didn't advertise a block size — firmware too old for BLE OTA.")
            return false
        }

        val chunk = ble.mtuPayload()
        progress.status("Streaming firmware over BLE… (MTU payload ${chunk}B)")
        var sent = 0
        while (sent < image.size) {
            val blockEnd = minOf(sent + block, image.size)
            var off = sent
            while (off < blockEnd) {
                val n = minOf(chunk, blockEnd - off)
                if (!ble.writeChunkBlocking(image.copyOfRange(off, off + n), 6000)) {
                    progress.status("BLE stalled at $off / ${image.size} bytes — write not confirmed.\n\n" +
                        "Move closer to the radio and retry, or use USB.")
                    return false
                }
                off += n
            }
            sent = blockEnd
            if (!waitForAck(sent, 10000, progress)) {
                val term = pendingTerminal
                if (term != null) { progress.status("Radio aborted the flash: $term"); return false }
                progress.status(
                    "BLE stalled at $sent / ${image.size} bytes — no ack from the radio.\n\n" +
                    "Keep the phone close and retry, or fall back to USB."
                )
                return false
            }
        }

        progress.status("Verifying & finalizing…")
        val done = waitForResult(45000, progress) ?: run {
            progress.status(
                "No completion response.\n\n" +
                "The radio may still be verifying/rebooting — give it a few seconds. " +
                "If it doesn't come back, reflash over USB."
            )
            return false
        }
        val ok = done.contains("\"ok\":true")
        progress.status(
            when {
                !ok -> "Flash failed: $done"
                done.contains("\"staged\":1") ->
                    "Image verified & staged — the radio is rebooting into recovery to install it. " +
                    "Watch the radio screen; it reboots a couple of times. Don't power off."
                else -> "Done — radio rebooting into new firmware"
            }
        )
        return ok
    }

    // ── Compression ──────────────────────────────────────────────────────────

    private fun compress(data: ByteArray): ByteArray {
        // nowrap=true → raw deflate (no zlib header/trailer); firmware uses tinfl with no PARSE_ZLIB_HEADER flag.
        val deflater = Deflater(Deflater.DEFAULT_COMPRESSION, true)
        deflater.setInput(data)
        deflater.finish()
        val out = ByteArrayOutputStream(maxOf(data.size / 2, 4096))
        val buf = ByteArray(65536)
        while (!deflater.finished()) {
            val n = deflater.deflate(buf)
            if (n > 0) out.write(buf, 0, n)
        }
        deflater.end()
        return out.toByteArray()
    }

    // ── Protocol helpers (mirror SerialOta) ───────────────────────────────────

    private var pendingTerminal: String? = null

    private fun preflight(progress: Progress): Boolean {
        for (attempt in 1..6) {
            progress.status("Checking BLE link to the radio… (try $attempt)")
            if (!writeLine("{\"cmd\":\"ping\"}")) {
                progress.status("BLE write failed — is the radio connected and on?")
                return false
            }
            if (waitForLine({ it.contains("pong") }, 1000) != null) return true
        }
        progress.status(
            "No response from the radio over BLE.\n\n" +
            "Reconnect (Settings → Scan) and try again, or flash over USB."
        )
        return false
    }

    /** Wait until the firmware acks ≥ [target] bytes; drives percent from the ack. */
    private fun waitForAck(target: Int, timeoutMs: Long, progress: Progress): Boolean {
        pendingTerminal = null
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            val line = lines.poll(200, TimeUnit.MILLISECONDS) ?: continue
            val ack = Regex("\"ack\":(\\d+)").find(line)?.groupValues?.get(1)?.toLongOrNull()
            when {
                ack != null -> {
                    val total = Regex("\"total\":(\\d+)").find(line)?.groupValues?.get(1)?.toLongOrNull()
                    if (total != null && total > 0)
                        progress.percent((ack * 100 / total).toInt().coerceIn(0, 100))
                    if (ack >= target) return true
                }
                line.contains("\"ok\"") -> { pendingTerminal = line; return false }
            }
        }
        return false
    }

    /** Read lines until a terminal result (begin ack / completion / error). */
    private fun waitForResult(timeoutMs: Long, progress: Progress): String? {
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            val line = lines.poll(200, TimeUnit.MILLISECONDS) ?: continue
            when {
                line.contains("\"ok\"") -> return line
                line.contains("\"fin\"") -> {
                    progress.percent(100)
                    progress.status("Radio verifying image & switching boot slot — don't power off…")
                }
            }
        }
        return null
    }

    private fun waitForLine(pred: (String) -> Boolean, timeoutMs: Long): String? {
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            val line = lines.poll(100, TimeUnit.MILLISECONDS) ?: continue
            if (pred(line)) return line
        }
        return null
    }

    private fun writeLine(s: String): Boolean =
        ble.writeChunkBlocking((s + "\n").toByteArray(), 3000)
}
