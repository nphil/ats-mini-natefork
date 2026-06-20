package com.atsmini.remote.usb

import com.hoho.android.usbserial.driver.UsbSerialPort
import java.io.ByteArrayOutputStream

/**
 * Minimal ESP32-S3 serial ROM bootloader flasher (esptool protocol subset:
 * SLIP framing + SYNC / SPI_ATTACH / FLASH_BEGIN / FLASH_DATA / FLASH_END).
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

    private val ESP_SYNC         = 0x08
    private val ESP_SPI_ATTACH   = 0x0D
    private val ESP_FLASH_BEGIN  = 0x02
    private val ESP_FLASH_DATA   = 0x03
    private val ESP_FLASH_END    = 0x04

    // 1 KiB blocks — required by ROM (stub uses 4 KiB, but we have no stub).
    private val FLASH_BLOCK = 0x400

    fun flash(offset: Int, image: ByteArray, progress: Progress): Boolean {
        // Try to sync first — device may already be in download mode (user pressed BOOT+RESET).
        progress.status("Connecting to ROM bootloader…")
        if (!sync(progress, attempts = 3)) {
            // Not responding yet. Try auto-reset and give it more attempts.
            progress.status("Sending auto-reset…")
            enterBootloader()
            if (!sync(progress, attempts = 10)) {
                progress.status(
                    "ROM bootloader not responding.\n\n" +
                    "Manually enter bootloader mode:\n" +
                    "1. Hold the BOOT button\n" +
                    "2. Press and release RESET\n" +
                    "3. Release BOOT\n\n" +
                    "Then tap the flash button again."
                )
                return false
            }
        }

        progress.status("Attaching SPI flash…")
        if (command(ESP_SPI_ATTACH, ByteArray(8), 0) == null) {
            progress.status("SPI_ATTACH failed — check connection and bootloader mode")
            return false
        }

        val numBlocks = (image.size + FLASH_BLOCK - 1) / FLASH_BLOCK
        val eraseSize = numBlocks * FLASH_BLOCK  // must be block-aligned, not raw image.size
        val sizeMb = image.size / 1024 / 1024

        val beginData = ByteArrayOutputStream().apply {
            writeLe(eraseSize)
            writeLe(numBlocks)
            writeLe(FLASH_BLOCK)
            writeLe(offset)
            writeLe(0)  // encrypted = false — ESP32-S3 ROM requires this 5th field
        }.toByteArray()

        progress.status("Erasing $sizeMb MB flash… (may take several minutes)")
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
            progress.percent(sent * 100 / image.size)
            if (seq % 64 == 0) {
                progress.status("Writing… ${sent / 1024} / ${image.size / 1024} KB")
            }
        }

        progress.status("Finishing…")
        command(ESP_FLASH_END, ByteArrayOutputStream().apply { writeLe(0) }.toByteArray(), 0)
        progress.percent(100)
        progress.status("Done — radio rebooting into new firmware")
        return true
    }

    // --- Reset sequences --------------------------------------------------------

    private fun enterBootloader() {
        runCatching {
            // Classic auto-reset for USB-to-UART bridges (CH340, CP210x, FTDI).
            // DTR → GPIO0 (BOOT), RTS → EN (RESET) through bridge hardware.
            port.setDTR(false); port.setRTS(true);  sleep(100)
            port.setDTR(true);  port.setRTS(false); sleep(50)
            port.setDTR(false);                     sleep(50)
        }
        runCatching {
            // ESP32-S3 native USB-JTAG / USB-Serial CDC.
            // The IDF USB CDC driver watches line-state transitions to detect the
            // esptool double-reset pattern and calls esp_restart() into download mode.
            port.setRTS(false); port.setDTR(false); sleep(100)
            port.setDTR(true);                      sleep(100)
            port.setDTR(false); port.setRTS(true);  sleep(100)
            port.setRTS(false);                     sleep(200)
        }
        drain()
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
                // ROM response layout: [0x01, op, sz_lo, sz_hi, val×4, data...]
                // The error flag is the second-to-last byte of the full response.
                // (data portion is [..., error_byte, status_byte])
                val statusOk = resp.size < 10 || resp[resp.size - 2].toInt() == 0
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
