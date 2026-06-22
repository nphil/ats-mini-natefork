package com.atsmini.remote.usb

import com.hoho.android.usbserial.driver.UsbSerialPort
import java.io.ByteArrayOutputStream

/**
 * Minimal ESP32-S3 serial ROM bootloader flasher (esptool protocol subset:
 * SLIP framing + SYNC / SPI_ATTACH / SPI_SET_PARAMS / FLASH_BEGIN / FLASH_DATA /
 * FLASH_END).
 *
 * No stub loader and no compression — slower than desktop esptool but simple
 * and dependency-free. Works on a port handed over by [UsbSerialManager.takePortForFlashing].
 *
 * Protocol reference: esptool.py ESP32S3ROM class + SLIP framing spec.
 */
class EspFlasher(private val port: UsbSerialPort) {

    interface Progress {
        fun status(message: String)
        fun percent(value: Int)
    }

    // Short timeout for commands that return quickly.
    private val ROM_TIMEOUT = 3_000

    // FLASH_BEGIN erases flash before returning. At ~40 s/MB, 8 MB = ~320 s.
    // We use 360 s (6 min) to be safe on slow chips.
    private val ERASE_TIMEOUT = 360_000

    private val ESP_SYNC          = 0x08
    private val ESP_SPI_ATTACH    = 0x0D
    private val ESP_SPI_SET_PARAMS = 0x0B
    private val ESP_FLASH_BEGIN   = 0x02
    private val ESP_FLASH_DATA    = 0x03
    private val ESP_FLASH_END     = 0x04

    // 1 KiB blocks — required by ROM (stub uses 16 KiB, but we have no stub).
    private val FLASH_BLOCK = 0x400

    // ATS-Mini hardware is an ESP32-S3 with 8 MB QIO flash (both OSPI and QSPI
    // PSRAM variants). The ROM bootloader needs to be told this geometry via
    // SPI_SET_PARAMS or it writes with default params and the image bootloops.
    private val FLASH_SIZE_BYTES = 8 * 1024 * 1024

    /**
     * Flash a single [image] at [offset].
     * If [skipAutoReset] is true, skips the DTR/RTS reset sequence and only attempts
     * direct ROM sync — use this when the caller has already triggered reset externally.
     */
    fun flash(offset: Int, image: ByteArray, progress: Progress, skipAutoReset: Boolean = false): Boolean =
        flashParts(listOf(offset to image), progress, skipAutoReset)

    /** Flash multiple partitions in a single session.
     *
     *  [parts] is a list of (flashOffset, imageBytes) pairs. Parts are sorted by
     *  offset before writing so callers can pass them in any order.
     *  [skipAutoReset] — when true, does not send the DTR/RTS reset sequence.
     */
    fun flashParts(parts: List<Pair<Int, ByteArray>>, progress: Progress, skipAutoReset: Boolean = false): Boolean {
        if (!syncAndAttach(progress, skipAutoReset)) return false

        val sorted = parts.sortedBy { it.first }
        val totalBytes = sorted.sumOf { it.second.size }
        var alreadyFlashed = 0

        for ((offset, image) in sorted) {
            if (!writeImage(offset, image, alreadyFlashed, totalBytes, progress)) return false
            alreadyFlashed += image.size
        }

        // Leave flash download mode and reboot — ONCE, after every partition is
        // written. (Sending FLASH_END per-partition would reboot the chip out of
        // the bootloader before the remaining partitions are flashed.)
        flashFinish()

        progress.percent(100)
        progress.status("Done — radio rebooting into new firmware")
        return true
    }

    /** Leave the ROM download mode and reboot into the freshly flashed app. */
    private fun flashFinish() {
        // FLASH_END payload: 0 = reboot/run app, 1 = stay in loader.
        command(ESP_FLASH_END, ByteArrayOutputStream().apply { writeLe(0) }.toByteArray(), 0)
    }

    // --- Sync + SPI attach ------------------------------------------------------

    private fun syncAndAttach(progress: Progress, skipAutoReset: Boolean = false): Boolean {
        // Try to sync first — device may already be in download mode.
        progress.status("Connecting to ROM bootloader…")
        if (!sync(progress, attempts = 7)) {
            if (skipAutoReset) {
                // Caller handles reset externally; just report failure.
                progress.status("ROM bootloader not responding — retrying after reconnect")
                return false
            }
            // Not responding. Try DTR/RTS reset and give more attempts.
            progress.status("Triggering download mode via USB…")
            enterBootloader()
            // After reset the device briefly disconnects; give it time to re-enumerate.
            sleep(500)
            if (!sync(progress, attempts = 10)) {
                progress.status(
                    "ROM bootloader not responding.\n\n" +
                    "If the device reconnected, grant USB permission and tap Flash again.\n\n" +
                    "Manual entry: Hold BOOT → tap RESET → release BOOT → tap Flash."
                )
                return false
            }
        }

        progress.status("Attaching SPI flash…")
        // ROM SPI_ATTACH takes 8 bytes: <hspi_arg=0> + <is_legacy=0, 0, 0, 0>.
        if (command(ESP_SPI_ATTACH, ByteArray(8), 0) == null) {
            progress.status("SPI_ATTACH failed — check connection and bootloader mode")
            return false
        }

        progress.status("Configuring flash parameters…")
        // Tell the ROM the real flash geometry. esptool always does this on the
        // ESP32-S3 ROM; skipping it leaves the ROM on default params and the
        // written image bootloops even though every block "succeeds".
        val params = ByteArrayOutputStream().apply {
            writeLe(0)                  // fl_id
            writeLe(FLASH_SIZE_BYTES)   // total_size (8 MB)
            writeLe(64 * 1024)          // block_size
            writeLe(4 * 1024)           // sector_size
            writeLe(256)                // page_size
            writeLe(0xFFFF)             // status_mask
        }.toByteArray()
        if (command(ESP_SPI_SET_PARAMS, params, 0) == null) {
            progress.status("SPI_SET_PARAMS failed — check connection and bootloader mode")
            return false
        }
        return true
    }

    // --- Write one partition image -----------------------------------------------

    private fun writeImage(
        offset: Int,
        image: ByteArray,
        alreadyFlashed: Int,
        totalBytes: Int,
        progress: Progress,
    ): Boolean {
        val numBlocks = (image.size + FLASH_BLOCK - 1) / FLASH_BLOCK
        val eraseSize = image.size  // esptool sends the raw image size (erase is sector-granular)
        val sizeMb = image.size / 1024 / 1024

        val beginData = ByteArrayOutputStream().apply {
            writeLe(eraseSize)
            writeLe(numBlocks)
            writeLe(FLASH_BLOCK)
            writeLe(offset)
            writeLe(0)  // encrypted = false — ESP32-S3 ROM requires this 5th field
        }.toByteArray()

        val label = if (sizeMb > 0) "$sizeMb MB" else "${image.size / 1024} KB"
        progress.status("Erasing $label at 0x${offset.toString(16)}… (may take several minutes)")
        if (command(ESP_FLASH_BEGIN, beginData, 0, timeout = ERASE_TIMEOUT) == null) {
            progress.status(
                "FLASH_BEGIN failed.\n" +
                "Make sure the device is in ROM bootloader mode:\n" +
                "Hold BOOT → press RESET → release RESET → release BOOT"
            )
            return false
        }

        var seq = 0
        var sent = 0
        while (sent < image.size) {
            val len = minOf(FLASH_BLOCK, image.size - sent)
            val block = ByteArray(FLASH_BLOCK) { 0xFF.toByte() }
            System.arraycopy(image, sent, block, 0, len)

            val payload = ByteArrayOutputStream().apply {
                writeLe(FLASH_BLOCK); writeLe(seq); writeLe(0); writeLe(0)
                write(block)
            }.toByteArray()

            if (command(ESP_FLASH_DATA, payload, checksum(block)) == null) {
                progress.status("Write failed at block $seq (byte offset ${offset + sent})")
                return false
            }

            seq++
            sent += len
            val overallSent = alreadyFlashed + sent
            if (totalBytes > 0) progress.percent(overallSent * 100 / totalBytes)
            if (seq % 64 == 0) {
                progress.status("Writing… ${overallSent / 1024} / ${totalBytes / 1024} KB")
            }
        }

        return true
    }

    // --- Reset sequences --------------------------------------------------------

    private fun enterBootloader() {
        // The ATS-Mini uses the ESP32-S3's built-in USB-Serial/JTAG, which needs
        // esptool's USB-JTAG reset sequence (NOT the classic UART DTR/RTS one).
        // This is how a PC enters download mode over USB with no BOOT button.
        usbJtagReset()
        sleep(400)              // let the chip re-enumerate as the ROM USB device
        // Fallback for any board wired through a UART bridge (CH340/CP210x/FTDI).
        if (!quickProbe()) {
            uartReset()
            sleep(400)
        }
        drain()
    }

    /** esptool USBJTAGReset: drives the S3 USB-Serial/JTAG into download mode. */
    private fun usbJtagReset() {
        runCatching {
            port.setRTS(false); port.setDTR(false); sleep(100)  // idle
            port.setDTR(true);  port.setRTS(false); sleep(100)  // assert IO0 (GPIO0 low)
            port.setRTS(true);  port.setDTR(false); sleep(100)  // reset, IO0 still requested
            port.setRTS(true)                                    // (Windows quirk: re-set RTS)
            sleep(100)
            port.setDTR(false); port.setRTS(false)               // release
        }
    }

    /** Classic UART auto-reset (DTR→GPIO0, RTS→EN), GPIO0 held low through reset release. */
    private fun uartReset() {
        runCatching {
            port.setDTR(false); port.setRTS(false); sleep(50)
            port.setDTR(true);                      sleep(100)  // GPIO0 = LOW
            port.setRTS(true);                      sleep(50)   // EN = LOW (hold in reset)
            port.setRTS(false);                     sleep(50)   // EN = HIGH, GPIO0 still LOW
            port.setDTR(false)                                   // release GPIO0
        }
    }

    /** Single fast SYNC probe (no status spam) to see if the ROM is responding. */
    private fun quickProbe(): Boolean {
        val data = ByteArrayOutputStream().apply {
            write(byteArrayOf(0x07, 0x07, 0x12, 0x20))
            repeat(32) { write(0x55) }
        }.toByteArray()
        return command(ESP_SYNC, data, 0, retries = 1, timeout = 300) != null
    }

    // --- SYNC ------------------------------------------------------------------

    private fun sync(progress: Progress, attempts: Int): Boolean {
        val data = ByteArrayOutputStream().apply {
            write(byteArrayOf(0x07, 0x07, 0x12, 0x20))
            repeat(32) { write(0x55) }
        }.toByteArray()

        repeat(attempts) { attempt ->
            progress.status("Syncing with ROM bootloader (${attempt + 1}/$attempts)…")
            if (command(ESP_SYNC, data, 0, retries = 1) != null) {
                repeat(7) { readFrame(200) }  // drain extra SYNC echoes the ROM sends
                return true
            }
            sleep(100)
        }
        return false
    }

    // --- Command / SLIP --------------------------------------------------------

    private fun command(
        op: Int,
        data: ByteArray,
        chk: Int,
        retries: Int = 2,
        timeout: Int = ROM_TIMEOUT,
    ): ByteArray? {
        repeat(retries) {
            writeFrame(buildPacket(op, data, chk))
            val resp = readFrame(timeout)
            if (resp != null && resp.size >= 8 && (resp[1].toInt() and 0xFF) == op) {
                // ROM response layout: [0x01, op, sz_lo, sz_hi, val×4, status...]
                // The ESP32-S3 ROM returns 4 trailing status bytes; only the
                // FIRST (resp[8]) carries the result — 0 = success, non-zero =
                // failure. The last two are reserved (always 0), so the old
                // resp[size-2] check looked at a reserved byte and never caught
                // a rejected block. SYNC is matched on opcode only (esptool does
                // not status-check it).
                val statusOk = op == ESP_SYNC || resp.size < 10 || resp[8].toInt() == 0
                if (statusOk) return resp
            }
        }
        return null
    }

    private fun buildPacket(op: Int, data: ByteArray, chk: Int): ByteArray {
        val out = ByteArrayOutputStream()
        out.write(0x00)                             // direction: request
        out.write(op)
        out.write(data.size and 0xFF)               // length low
        out.write((data.size shr 8) and 0xFF)       // length high
        out.write(chk and 0xFF)                     // checksum (4 bytes LE)
        out.write((chk shr 8) and 0xFF)
        out.write((chk shr 16) and 0xFF)
        out.write((chk shr 24) and 0xFF)
        out.write(data)
        return out.toByteArray()
    }

    private fun writeFrame(packet: ByteArray) {
        runCatching {
            val out = ByteArrayOutputStream()
            out.write(0xC0)
            for (b in packet) when (b.toInt() and 0xFF) {
                0xC0 -> { out.write(0xDB); out.write(0xDC) }
                0xDB -> { out.write(0xDB); out.write(0xDD) }
                else -> out.write(b.toInt())
            }
            out.write(0xC0)
            port.write(out.toByteArray(), 1000)
        }
        // Silently ignore write errors — the port may have briefly disconnected
        // during a device reset; sync() retries will detect and report failure.
    }

    private fun readFrame(timeoutMs: Int): ByteArray? {
        val buf = ByteArray(512)
        val frame = ByteArrayOutputStream()
        var started = false
        var escape = false
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            val n = runCatching { port.read(buf, 200) }.getOrDefault(0)
            for (i in 0 until n) {
                val v = buf[i].toInt() and 0xFF
                when {
                    !started -> if (v == 0xC0) started = true
                    escape   -> { frame.write(if (v == 0xDC) 0xC0 else 0xDB); escape = false }
                    v == 0xDB -> escape = true
                    v == 0xC0 -> if (frame.size() > 0) return frame.toByteArray()
                    else       -> frame.write(v)
                }
            }
        }
        return null
    }

    private fun drain() {
        val buf = ByteArray(512)
        repeat(5) { runCatching { port.read(buf, 50) } }
    }

    private fun checksum(data: ByteArray): Int {
        var c = 0xEF
        for (b in data) c = c xor (b.toInt() and 0xFF)
        return c
    }

    private fun ByteArrayOutputStream.writeLe(value: Int) {
        write(value and 0xFF)
        write((value shr 8) and 0xFF)
        write((value shr 16) and 0xFF)
        write((value shr 24) and 0xFF)
    }

    private fun sleep(ms: Long) = runCatching { Thread.sleep(ms) }
}
