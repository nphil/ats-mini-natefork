package com.atsmini.remote.data

import org.json.JSONObject
import java.util.Calendar

/** Builders for the ATS-Mini JSON line protocol (same commands as the iOS app). */
object Protocol {
    private fun obj(vararg pairs: Pair<String, Any>): String {
        val o = JSONObject()
        for ((k, v) in pairs) o.put(k, v)
        return o.toString()
    }

    fun subscribe(ms: Int) = obj("cmd" to "sub", "ms" to ms)
    fun status() = obj("cmd" to "status")
    fun options() = obj("cmd" to "opts")

    fun frequency(freq: Int) = obj("cmd" to "freq", "val" to freq)
    fun delta(cmd: String, d: Int) = obj("cmd" to cmd, "d" to d)
    fun seek(direction: Int) = obj("cmd" to "seek", "d" to direction)

    // Raw encoder-rotate single chars (one tuning step up/down), like the iOS app.
    fun rotateUp() = "R"
    fun rotateDown() = "r"
    fun volume(d: Int) = obj("cmd" to "vol", "d" to d)
    fun click() = obj("cmd" to "click")
    fun sleep(on: Boolean) = obj("cmd" to "sleep", "on" to if (on) 1 else 0)
    fun scan(step: Int) = obj("cmd" to "scan", "step" to step)

    fun waterfallStart(sf: Int? = null, step: Int? = null, n: Int? = null): String {
        val o = JSONObject().put("cmd", "wf_start")
        sf?.let { o.put("sf", it) }
        step?.let { o.put("step", it) }
        n?.let { o.put("n", it) }
        return o.toString()
    }
    fun waterfallStop() = obj("cmd" to "wf_stop")

    fun listPresets() = obj("cmd" to "list_presets")
    fun savePreset(name: String) = obj("cmd" to "save_preset", "name" to name)
    fun loadPreset(idx: Int) = obj("cmd" to "load_preset", "idx" to idx)
    fun deletePreset(idx: Int) = obj("cmd" to "delete_preset", "idx" to idx)
    fun renamePreset(idx: Int, name: String) = obj("cmd" to "rename_preset", "idx" to idx, "name" to name)

    fun setTimeNow(): String {
        val c = Calendar.getInstance()
        return obj(
            "cmd" to "settime",
            "hh" to c.get(Calendar.HOUR_OF_DAY),
            "mm" to c.get(Calendar.MINUTE),
            "ss" to c.get(Calendar.SECOND),
        )
    }
}
