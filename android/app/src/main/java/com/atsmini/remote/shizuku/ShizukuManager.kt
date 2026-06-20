package com.atsmini.remote.shizuku

import android.content.pm.PackageManager
import com.atsmini.remote.data.LogType
import com.atsmini.remote.data.RadioRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import rikka.shizuku.Shizuku
import java.io.BufferedReader
import java.io.InputStreamReader

/**
 * Optional privileged helper. When Shizuku is running, the app gains
 * shell-level powers a sandboxed iOS app can never have: silently granting USB
 * permission, auto-joining the "ATS-Mini Recovery" Wi-Fi AP, and toggling the
 * radios when connecting. Everything degrades gracefully if Shizuku is absent.
 */
object ShizukuManager {

    private const val PERMISSION_CODE = 4711

    private val _available = MutableStateFlow(false)
    val available: StateFlow<Boolean> = _available.asStateFlow()

    private val _granted = MutableStateFlow(false)
    val granted: StateFlow<Boolean> = _granted.asStateFlow()

    private val binderListener = Shizuku.OnBinderReceivedListener { refresh() }
    private val deadListener = Shizuku.OnBinderDeadListener { _available.value = false }
    private val permListener = Shizuku.OnRequestPermissionResultListener { code, result ->
        if (code == PERMISSION_CODE) _granted.value = result == PackageManager.PERMISSION_GRANTED
    }

    fun init() {
        runCatching {
            Shizuku.addBinderReceivedListenerSticky(binderListener)
            Shizuku.addBinderDeadListener(deadListener)
            Shizuku.addRequestPermissionResultListener(permListener)
        }
        refresh()
    }

    fun refresh() {
        val ok = runCatching { Shizuku.pingBinder() }.getOrDefault(false)
        _available.value = ok
        _granted.value = ok && runCatching {
            Shizuku.checkSelfPermission() == PackageManager.PERMISSION_GRANTED
        }.getOrDefault(false)
    }

    fun requestPermission() {
        if (!_available.value) return
        runCatching {
            if (Shizuku.checkSelfPermission() == PackageManager.PERMISSION_GRANTED) {
                _granted.value = true
            } else {
                Shizuku.requestPermission(PERMISSION_CODE)
            }
        }
    }

    /** Run a shell command with Shizuku (ADB-shell uid) and return its output. */
    fun exec(command: String): String {
        if (!_granted.value) return ""
        return runCatching {
            val m = Shizuku::class.java.getDeclaredMethod(
                "newProcess", Array<String>::class.java, Array<String>::class.java, String::class.java
            ).apply { isAccessible = true }
            val process = m.invoke(null, arrayOf("sh", "-c", command), null, null) as Process
            val out = BufferedReader(InputStreamReader(process.inputStream)).readText()
            val err = BufferedReader(InputStreamReader(process.errorStream)).readText()
            process.waitFor()
            (out + err).trim()
        }.getOrElse { e ->
            RadioRepository.log("Shizuku exec failed: ${e.message}", LogType.ERROR); ""
        }
    }

    fun enableBluetooth() { exec("svc bluetooth enable") }
    fun enableWifi() { exec("svc wifi enable") }

    /** Auto-join a Wi-Fi network (used for the radio's recovery AP). */
    fun connectWifi(ssid: String, password: String) {
        enableWifi()
        val out = exec("cmd wifi connect-network \"$ssid\" wpa2 \"$password\"")
        RadioRepository.log("Wi-Fi join $ssid: ${out.ifEmpty { "requested" }}")
    }

    /** Silently grant this app permission to the attached USB device. */
    fun grantUsbPermission(deviceName: String) {
        // ADB-shell uid can broadcast the USB permission grant.
        exec("am broadcast -a com.atsmini.remote.USB_PERMISSION --es device \"$deviceName\"")
    }

    fun version(): String =
        if (_available.value) runCatching { "API ${Shizuku.getVersion()}" }.getOrDefault("running")
        else "not running"
}
