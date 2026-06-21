package com.atsmini.remote.net

import android.content.Context
import java.io.File

/**
 * On-device cache of recently used firmware images. Keeps at most
 * [MAX_ENTRIES] files in app-private storage so the radio can be flashed or
 * OTA-updated with no internet — e.g. a tablet joined to the recovery adhoc AP,
 * which has no upstream connection. Eviction is least-recently-used by file
 * modification time (re-reading or re-storing an entry touches it).
 *
 * Stored under filesDir/fw_cache. File names are the GitHub asset names
 * (already filesystem-safe: letters, digits, '.', '_', '-'), so the kind of
 * image (full / flash / recovery / ota) can be inferred straight from the name.
 */
object FirmwareCache {
    const val MAX_ENTRIES = 15
    private const val DIR = "fw_cache"

    @Volatile private var dir: File? = null

    data class Entry(val name: String, val size: Long, val modified: Long)

    fun init(context: Context) {
        dir = File(context.filesDir, DIR).apply { mkdirs() }
    }

    private fun safeName(name: String): String = name.replace(Regex("[^A-Za-z0-9._-]"), "_")

    /** Persist [bytes] under [name]; touch on re-cache; evict oldest beyond MAX_ENTRIES. */
    fun put(name: String, bytes: ByteArray): Boolean {
        val d = dir ?: return false
        return runCatching {
            val f = File(d, safeName(name))
            f.writeBytes(bytes)
            f.setLastModified(System.currentTimeMillis())
            prune()
            true
        }.getOrDefault(false)
    }

    /** Read a cached image by name, touching it so it is treated as recently used. */
    fun read(name: String): ByteArray? {
        val d = dir ?: return null
        val f = File(d, safeName(name))
        if (!f.isFile) return null
        return runCatching {
            f.setLastModified(System.currentTimeMillis())
            f.readBytes()
        }.getOrNull()
    }

    fun has(name: String): Boolean {
        val d = dir ?: return false
        return File(d, safeName(name)).isFile
    }

    /** Cached entries, most-recently-used first. */
    fun list(): List<Entry> {
        val d = dir ?: return emptyList()
        return (d.listFiles() ?: emptyArray())
            .filter { it.isFile }
            .sortedByDescending { it.lastModified() }
            .map { Entry(it.name, it.length(), it.lastModified()) }
    }

    fun delete(name: String): Boolean {
        val d = dir ?: return false
        return File(d, safeName(name)).delete()
    }

    private fun prune() {
        val d = dir ?: return
        (d.listFiles() ?: return)
            .filter { it.isFile }
            .sortedByDescending { it.lastModified() }
            .drop(MAX_ENTRIES)
            .forEach { runCatching { it.delete() } }
    }
}
