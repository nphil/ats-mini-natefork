package com.atsmini.remote.usb

import com.hoho.android.usbserial.driver.UsbSerialPort
import java.io.ByteArrayOutputStream

/**
 * Minimal ESP32-S3 serial ROM bootloader flasher (esptool protocol subset:
 * SLIP framing + SYNC / SPI_ATTACH / FLASH_BEGIN / FLASH_DATA / FLASH_END).
 *
 * No stub loader and no compression — slower than desktop esptool but simple
 * and dependency-free, which is what a one-tap recovery tool wants. Works on a
 * port already opened by [UsbSerialManager.takePortForFlashing].
 */
class EspFlasher(private val port: UsbSerialPort) {

    interface Progress {
        fun status(message: String)
        fun percent(value: Int)
    }

    private val readTimeoutMs = 3000

    // ROM commands
    private val ESP_SYNC = 0x08
    private val ESP_SPI_ATTACH = 0x0D
    private val ESP_FLASH_BEGIN = 0x02
    private val ESP_FLASH_DATA = 0x03
    private val ESP_FLASH_END = 0x04

    private val FLASH_BLOCK = 0x400 // 1 KiB blocks (conservative, no-stub)

    fun flash(offset: Int, image: ByteArray, progress: Progress): Boolean {
        progress.status("Resetting into download mode…")
        enterBootloader()
        if (!sync(progress)) { progress.status("No response from ROM bootloader"); return false }

        progress.status("Configuring SPI flash…")
        command(ESP_SPI_ATTACH, ByteArray(8), 0)

        val numBlocks = (image.size + FLASH_BLOCK - 1) / FLASH_BLOCK
        val beginData = ByteArrayOutputStream().apply {
            writeLe(image.size); writeLe(numBlocks); writeLe(FLASH_BLOCK); writeLe(offset)
        }.toByteArray()
        progress.status("Erasing flash…")
        if (command(ESP_FLASH_BEGIN, beginData, 0) == null) {
            progress.status("FLASH_BEGIN failed"); return false
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
            val csum = checksum(block)
            if (command(ESP_FLASH_DATA, payload, csum) == null) {
                progress.status("Write failed at block $seq"); return false
            }
            seq++; sent += len
            progress.percent((sent * 100 / image.size))
        }

        progress.status("Finishing…")
        // reboot flag = 0 → run user app after flashing
        val endData = ByteArrayOutputStream().apply { writeLe(0) }.toByteArray()
        command(ESP_FLASH_END, endData, 0)
        progress.percent(100)
        progress.status("Done — radio rebooting into new firmware")
        return true
    }

    // --- Reset sequences --------------------------------------------------------

    private fun enterBootloader() {
        // Classic auto-reset (USB-serial bridge boards).
        runCatching {
            port.setDTR(false); port.setRTS(true); sleep(100)
            port.setDTR(true); port.setRTS(false); sleep(50)
            port.setDTR(false); sleep(50)
        }
        // ESP32-S3 native USB-Serial/JTAG reset (toggle both lines).
        runCatching {
            port.setRTS(false); port.setDTR(false); sleep(100)
            port.setDTR(true); port.setRTS(false); sleep(100)
            port.setRTS(true); port.setDTR(false); sleep(100)
            port.setDTR(false); port.setRTS(false); sleep(100)
        }
        drain()
    }

    private fun sync(progress: Progress): Boolean {
        val data = ByteArrayOutputStream().apply {
            write(byteArrayOf(0x07, 0x07, 0x12, 0x20))
            repeat(32) { write(0x55) }
        }.toByteArray()
        repeat(7) { attempt ->
            progress.status("Syncing with bootloader (${attempt + 1}/7)…")
            val resp = command(ESP_SYNC, data, 0, retries = 1)
            if (resp != null) {
                // Drain any extra SYNC echoes.
                repeat(7) { readFrame(200) }
                return true
            }
            sleep(100)
        }
        return false
    }

    // --- Command / SLIP ---------------------------------------------------------

    private fun command(op: Int, data: ByteArray, chk: Int, retries: Int = 2): ByteArray? {
        repeat(retries) {
            writeFrame(buildPacket(op, data, chk))
            val resp = readFrame(readTimeoutMs)
            if (resp != null && resp.size >= 8 && (resp[1].toInt() and 0xFF) == op) {
                // Trailing status: byte 0 of the last 4 bytes is the failure flag.
                val statusOk = resp.size < 10 || resp[resp.size - 4].toInt() == 0
                if (statusOk) return resp
            }
        }
        return null
    }

    private fun buildPacket(op: Int, data: ByteArray, chk: Int): ByteArray {
        val out = ByteArrayOutputStream()
        out.write(0x00)              // request direction
        out.write(op)
        out.write(data.size and 0xFF)
        out.write((data.size shr 8) and 0xFF)
        out.write(chk and 0xFF)
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
        val buf = ByteArray(256)
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
                    escape -> { frame.write(if (v == 0xDC) 0xC0 else 0xDB); escape = false }
                    v == 0xDB -> escape = true
                    v == 0xC0 -> if (frame.size() > 0) return frame.toByteArray()
                    else -> frame.write(v)
                }
            }
        }
        return null
    }

    private fun drain() {
        val buf = ByteArray(256)
        repeat(4) { runCatching { port.read(buf, 50) } }
    }

    private fun checksum(data: ByteArray): Int {
        var c = 0xEF
        for (b in data) c = c xor (b.toInt() and 0xFF)
        return c
    }

    private fun ByteArrayOutputStream.writeLe(value: Int) {
        write(value and 0xFF); write((value shr 8) and 0xFF)
        write((value shr 16) and 0xFF); write((value shr 24) and 0xFF)
    }

    private fun sleep(ms: Long) = runCatching { Thread.sleep(ms) }
}
