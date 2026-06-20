package com.atsmini.remote.data

import org.json.JSONObject

/** Which physical link is currently carrying the radio protocol. */
enum class Transport { NONE, BLE, USB }

enum class FreqUnit { AUTO, MHZ, KHZ }

enum class LogType { INFO, OK, ERROR }

data class LogEntry(
    val timestamp: Long,
    val message: String,
    val type: LogType,
)

data class ScanData(
    val startFreq: Int,
    val step: Int,
    val pointCount: Int,
    val rssi: List<Int>,
    val snr: List<Int>,
    val channels: List<Int>,
)

data class WaterfallMeta(val startFreq: Int, val step: Int, val pointCount: Int)

data class Preset(val idx: Int, val name: String, val channelCount: Int)

/**
 * Immutable snapshot of everything the radio reports over the line protocol.
 * Mirrors the iOS RadioState model so behaviour matches across platforms.
 */
data class RadioStatus(
    val transport: Transport = Transport.NONE,
    val connectionStatus: String = "Disconnected",

    val frequency: Int = 0,        // FM: x10kHz, AM/SSB: kHz
    val bfo: Int = 0,
    val mode: Int = 0,             // 0=FM 1=LSB 2=USB 3=AM
    val modeName: String = "---",
    val bandName: String = "---",
    val stepSize: String = "---",
    val bandwidth: String = "---",
    val agc: String = "--",

    val rssi: Int = 0,
    val snr: Int = 0,
    val cpu0: Int = 0,
    val cpu1: Int = 0,

    val batteryVoltage: Double = 0.0,
    val volume: Int = 35,

    val rdsStation: String = "",
    val rdsText: String = "",
    val rdsPTY: String = "",
    val rdsTime: String = "",

    val wifiIP: String = "",
    val wifiIsAP: Boolean = false,
    val firmwareVersion: Int = 0,

    val seq: Int = 0,
) {
    val isConnected: Boolean get() = transport != Transport.NONE
    val isFM: Boolean get() = mode == 0

    val batteryPercent: Double
        get() = ((batteryVoltage - 3.3) / (4.2 - 3.3) * 100).coerceIn(0.0, 100.0)

    fun resolvedUnit(pref: FreqUnit): FreqUnit = when (pref) {
        FreqUnit.AUTO -> if (isFM) FreqUnit.MHZ else FreqUnit.KHZ
        else -> pref
    }

    fun formattedFrequency(pref: FreqUnit): String {
        return when (resolvedUnit(pref)) {
            FreqUnit.MHZ -> {
                val mhz = if (isFM) frequency / 100.0 else frequency / 1000.0
                if (isFM) String.format("%.2f", mhz) else String.format("%.3f", mhz)
            }
            else -> {
                val khz = if (isFM) frequency * 10 else frequency
                khz.toString()
            }
        }
    }

    fun frequencyUnitLabel(pref: FreqUnit): String =
        if (resolvedUnit(pref) == FreqUnit.MHZ) "MHz" else "kHz"

    companion object {
        /** Apply a {"t":"s",...} status packet onto an existing snapshot. */
        fun applyStatus(prev: RadioStatus, msg: JSONObject): RadioStatus {
            var s = prev.copy(
                frequency = msg.optInt("f", prev.frequency),
                bfo = msg.optInt("bfo", prev.bfo),
                mode = msg.optInt("m", prev.mode),
                modeName = msg.optString("mn", prev.modeName),
                bandName = msg.optString("bn", prev.bandName),
                stepSize = msg.optString("st", prev.stepSize),
                bandwidth = msg.optString("bw", prev.bandwidth),
                rssi = msg.optInt("r", prev.rssi),
                snr = msg.optInt("sn", prev.snr),
                volume = msg.optInt("v", prev.volume),
                batteryVoltage = msg.optDouble("bat", prev.batteryVoltage),
                seq = msg.optInt("seq", prev.seq),
                cpu0 = msg.optInt("cpu0", prev.cpu0),
                cpu1 = msg.optInt("cpu1", prev.cpu1),
            )
            if (msg.has("agc")) s = s.copy(agc = msg.optInt("agc").toString())

            s = if (s.mode == 0) {
                s.copy(
                    rdsStation = msg.optString("ps", prev.rdsStation).trim(),
                    rdsText = msg.optString("rt", prev.rdsText).trim(),
                    rdsPTY = msg.optString("pty", prev.rdsPTY).trim(),
                    rdsTime = msg.optString("ct", prev.rdsTime),
                )
            } else {
                s.copy(rdsStation = "", rdsText = "", rdsPTY = "", rdsTime = "")
            }

            if (msg.has("wip")) s = s.copy(wifiIP = msg.optString("wip"))
            if (msg.has("wm")) s = s.copy(wifiIsAP = msg.optInt("wm") == 1)
            if (msg.has("fw")) s = s.copy(firmwareVersion = msg.optInt("fw"))
            return s
        }
    }
}
