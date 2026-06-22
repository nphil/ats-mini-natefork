package com.atsmini.remote.net

import java.io.DataOutputStream
import java.net.HttpURLConnection
import java.net.URL

/**
 * Uploads a firmware .bin to the radio's HTTP OTA endpoint (the recovery AP at
 * 192.168.4.1/update, or the running firmware's IP). Multipart field name
 * "firmware", matching Recovery.cpp's web upload handler.
 */
object RecoveryOta {
    const val RECOVERY_HOST = "192.168.4.1"

    // Recovery firmware soft-AP (ats-mini-recovery.ino).
    const val RECOVERY_SSID = "ATS-Recovery"
    const val RECOVERY_PASS = "ats12345"

    interface Progress {
        fun status(message: String)
        fun percent(value: Int)
    }

    /** Upload with up to 3 attempts — a "broken pipe" mid-transfer (the radio's
     *  async server briefly stalling under load) is usually transient. */
    fun upload(host: String, firmware: ByteArray, progress: Progress): Boolean {
        var lastErr = ""
        repeat(3) { attempt ->
            if (attempt > 0) {
                progress.status("Retrying upload (${attempt + 1}/3)…")
                Thread.sleep(1500)
            }
            val r = uploadOnce(host, firmware, progress)
            if (r.first) return true
            lastErr = r.second
            // A clean HTTP error (e.g. rejected image) won't improve on retry.
            if (r.second.startsWith("Failed (HTTP")) {
                progress.status(r.second); return false
            }
        }
        progress.status(
            "Upload failed: $lastErr\n\n" +
            "If you're on the same Wi-Fi, the most reliable path is to boot the radio into " +
            "recovery (hold the encoder at power-on) and use its Adhoc AP, or flash over USB."
        )
        return false
    }

    private fun uploadOnce(host: String, firmware: ByteArray, progress: Progress): Pair<Boolean, String> {
        val boundary = "----atsmini${System.currentTimeMillis()}"
        val url = URL("http://$host/update")
        progress.status("Connecting to $host…")
        val conn = (url.openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            doOutput = true
            connectTimeout = 10_000
            readTimeout = 180_000
            // Avoid a 100-continue round trip that some embedded servers mishandle.
            setRequestProperty("Expect", "")
            setRequestProperty("Connection", "close")
            setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
            setFixedLengthStreamingMode(multipartOverhead(boundary) + firmware.size)
        }
        return try {
            DataOutputStream(conn.outputStream).use { out ->
                out.writeBytes("--$boundary\r\n")
                out.writeBytes("Content-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n")
                out.writeBytes("Content-Type: application/octet-stream\r\n\r\n")
                // Smaller chunks with a flush each round let the radio's flash writes
                // keep pace and keep the TCP window from stalling (→ broken pipe).
                val chunk = 2048
                var sent = 0
                while (sent < firmware.size) {
                    val len = minOf(chunk, firmware.size - sent)
                    out.write(firmware, sent, len)
                    out.flush()
                    sent += len
                    progress.percent(sent * 100 / firmware.size)
                }
                out.writeBytes("\r\n--$boundary--\r\n")
                out.flush()
            }
            val code = conn.responseCode
            if (code in 200..299) {
                progress.status("Upload complete — radio rebooting")
                true to ""
            } else {
                false to "Failed (HTTP $code)"
            }
        } catch (e: Exception) {
            false to (e.message ?: "connection error")
        } finally {
            conn.disconnect()
        }
    }

    private fun multipartOverhead(boundary: String): Int {
        val head = "--$boundary\r\n" +
            "Content-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n" +
            "Content-Type: application/octet-stream\r\n\r\n"
        val tail = "\r\n--$boundary--\r\n"
        return head.toByteArray().size + tail.toByteArray().size
    }
}
