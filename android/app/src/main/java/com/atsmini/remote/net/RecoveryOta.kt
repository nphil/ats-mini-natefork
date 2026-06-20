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

    interface Progress {
        fun status(message: String)
        fun percent(value: Int)
    }

    fun upload(host: String, firmware: ByteArray, progress: Progress): Boolean {
        val boundary = "----atsmini${System.currentTimeMillis()}"
        val url = URL("http://$host/update")
        progress.status("Connecting to $host…")
        val conn = (url.openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            doOutput = true
            connectTimeout = 10_000
            readTimeout = 120_000
            setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
            setFixedLengthStreamingMode(
                multipartOverhead(boundary) + firmware.size
            )
        }
        return try {
            DataOutputStream(conn.outputStream).use { out ->
                out.writeBytes("--$boundary\r\n")
                out.writeBytes("Content-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n")
                out.writeBytes("Content-Type: application/octet-stream\r\n\r\n")
                val chunk = 4096
                var sent = 0
                while (sent < firmware.size) {
                    val len = minOf(chunk, firmware.size - sent)
                    out.write(firmware, sent, len)
                    sent += len
                    progress.percent(sent * 100 / firmware.size)
                }
                out.writeBytes("\r\n--$boundary--\r\n")
                out.flush()
            }
            val code = conn.responseCode
            progress.status(if (code in 200..299) "Upload complete — radio rebooting" else "Failed (HTTP $code)")
            code in 200..299
        } catch (e: Exception) {
            progress.status("Upload error: ${e.message}")
            false
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
