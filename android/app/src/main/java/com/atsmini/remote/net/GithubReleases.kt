package com.atsmini.remote.net

import org.json.JSONArray
import java.net.HttpURLConnection
import java.net.URL

/** Fetches ATS-Mini firmware assets from the GitHub releases of this repo. */
object GithubReleases {
    private const val REPO = "nphil/ats-mini-natefork"

    data class Asset(val name: String, val url: String, val size: Long)
    data class Firmware(val tag: String, val assets: List<Asset>) {
        val displayName: String get() = if (tag == "latest") "Latest build" else tag
        fun full() = assets.firstOrNull { it.name.endsWith("-ospi-full.bin") }
        fun flash() = assets.firstOrNull { it.name.endsWith("-ospi-flash.bin") }
        fun ota() = assets.firstOrNull { it.name.endsWith("-ospi-ota.bin") }
        fun recovery() = assets.firstOrNull { it.name.endsWith("-ospi-recovery.bin") }
    }

    /** Returns up to [count] releases. The "latest" pre-release is first if present,
     *  followed by non-pre-release versioned releases (tag matching ^v[0-9]). */
    fun listFirmware(count: Int = 10): List<Firmware> {
        val json = httpGet("https://api.github.com/repos/$REPO/releases?per_page=50") ?: return emptyList()
        val arr = runCatching { JSONArray(json) }.getOrNull() ?: return emptyList()

        val latestEntry = ArrayList<Firmware>()
        val versioned = ArrayList<Firmware>()

        for (i in 0 until arr.length()) {
            val rel = arr.optJSONObject(i) ?: continue
            val tag = rel.optString("tag_name")
            val isPrerelease = rel.optBoolean("prerelease", false)
            val assetsJson = rel.optJSONArray("assets") ?: continue
            val assets = ArrayList<Asset>()
            for (j in 0 until assetsJson.length()) {
                val a = assetsJson.optJSONObject(j) ?: continue
                if (a.optString("name").endsWith(".bin")) {
                    assets.add(Asset(a.optString("name"), a.optString("browser_download_url"), a.optLong("size")))
                }
            }
            if (assets.isEmpty()) continue

            val fw = Firmware(tag, assets)
            when {
                tag == "latest" && isPrerelease -> latestEntry.add(fw)
                Regex("^v[0-9]").containsMatchIn(tag) && !isPrerelease -> versioned.add(fw)
            }
        }

        val result = ArrayList<Firmware>()
        result.addAll(latestEntry)
        result.addAll(versioned)
        return result.take(count)
    }

    /** Latest non-prerelease firmware release (tag like vX.YY). */
    fun latestFirmware(): Firmware? = listFirmware(1).firstOrNull()

    /**
     * Download [asset], serving from the on-device cache when present (offline
     * friendly) and storing fresh downloads for later. Reports 100% immediately
     * on a cache hit.
     */
    fun downloadCached(asset: Asset, onProgress: (Int) -> Unit): ByteArray? {
        FirmwareCache.read(asset.name)?.let { onProgress(100); return it }
        val bytes = download(asset.url, onProgress) ?: return null
        FirmwareCache.put(asset.name, bytes)
        return bytes
    }

    fun download(url: String, onProgress: (Int) -> Unit): ByteArray? {
        return runCatching {
            val conn = (URL(url).openConnection() as HttpURLConnection).apply {
                instanceFollowRedirects = true
                connectTimeout = 15_000
                readTimeout = 60_000
            }
            val total = conn.contentLength
            conn.inputStream.use { input ->
                val out = java.io.ByteArrayOutputStream()
                val buf = ByteArray(16384)
                var read: Int
                var sum = 0
                while (input.read(buf).also { read = it } >= 0) {
                    out.write(buf, 0, read)
                    sum += read
                    if (total > 0) onProgress(sum * 100 / total)
                }
                out.toByteArray()
            }
        }.getOrNull()
    }

    private fun httpGet(url: String): String? = runCatching {
        val conn = (URL(url).openConnection() as HttpURLConnection).apply {
            setRequestProperty("Accept", "application/vnd.github+json")
            connectTimeout = 15_000; readTimeout = 30_000
        }
        conn.inputStream.bufferedReader().readText()
    }.getOrNull()
}
