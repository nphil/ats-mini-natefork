package com.atsmini.remote.usb

import android.app.PendingIntent
import android.content.Context
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.atsmini.remote.data.LogType
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.data.Transport
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.util.SerialInputOutputManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Wired USB-OTG transport. Carries the same JSON line protocol as BLE, and
 * additionally exposes the raw ESP32 console — including the boot and panic
 * output — which is impossible to reach on iOS.
 */
class UsbSerialManager(private val context: Context) {

    private val usbManager get() = context.getSystemService(Context.USB_SERVICE) as UsbManager

    private var port: UsbSerialPort? = null
    private var ioManager: SerialInputOutputManager? = null

    private val _attached = MutableStateFlow<UsbDevice?>(null)
    val attached: StateFlow<UsbDevice?> = _attached.asStateFlow()

    fun refresh() {
        _attached.value = firstDriverDevice()
    }

    private fun firstDriverDevice(): UsbDevice? =
        UsbSerialProber.getDefaultProber().findAllDrivers(usbManager).firstOrNull()?.device

    fun hasPermission(device: UsbDevice): Boolean = usbManager.hasPermission(device)

    fun requestPermission(device: UsbDevice, intent: PendingIntent) {
        usbManager.requestPermission(device, intent)
    }

    /** Open the first detected serial adapter for the JSON console session. */
    fun open(baud: Int = 115200): Boolean {
        val driver = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager).firstOrNull()
            ?: run { RadioRepository.log("No USB serial device found", LogType.ERROR); return false }
        val device = driver.device
        if (!usbManager.hasPermission(device)) {
            RadioRepository.log("USB permission required", LogType.ERROR); return false
        }
        val connection = usbManager.openDevice(device)
            ?: run { RadioRepository.log("Could not open USB device", LogType.ERROR); return false }
        val p = driver.ports.first()
        return try {
            p.open(connection)
            p.setParameters(baud, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
            runCatching { p.setDTR(true); p.setRTS(true) }
            port = p
            startReader(p)
            RadioRepository.attachTransport(Transport.USB) { line -> writeLine(line) }
            true
        } catch (e: Exception) {
            RadioRepository.log("USB open failed: ${e.message}", LogType.ERROR)
            runCatching { p.close() }
            false
        }
    }

    private fun startReader(p: UsbSerialPort) {
        val io = SerialInputOutputManager(p, object : SerialInputOutputManager.Listener {
            override fun onNewData(data: ByteArray) {
                val text = String(data)
                RadioRepository.appendConsole(text)
                RadioRepository.ingest(text)
            }
            override fun onRunError(e: Exception) {
                RadioRepository.log("USB read error: ${e.message}", LogType.ERROR)
                close()
            }
        })
        ioManager = io
        io.start()
    }

    fun writeLine(line: String) {
        val p = port ?: return
        runCatching { p.write((line + "\n").toByteArray(), 1000) }
    }

    fun close() {
        ioManager?.stop(); ioManager = null
        runCatching { port?.close() }; port = null
        if (RadioRepository.status.value.transport == Transport.USB) RadioRepository.detachTransport()
    }

    /**
     * Detach the console reader and hand the raw port to a flashing session.
     * Returns the open port (caller owns it until [resumeAfterFlash]).
     * Always (re-)applies 115200 8N1 so the ROM bootloader can communicate.
     */
    fun takePortForFlashing(): UsbSerialPort? {
        ioManager?.stop(); ioManager = null
        val existing = port
        val p = if (existing != null) {
            existing
        } else {
            val driver = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager).firstOrNull() ?: return null
            if (!usbManager.hasPermission(driver.device)) return null
            val connection = usbManager.openDevice(driver.device) ?: return null
            val newPort = driver.ports.first()
            newPort.open(connection)
            port = newPort
            newPort
        }
        // ROM bootloader always uses 115200 8N1; reset in case a prior session changed it.
        runCatching { p.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE) }
        return p
    }

    fun resumeAfterFlash() {
        val p = port ?: return
        runCatching { p.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE) }
        startReader(p)
    }
}
