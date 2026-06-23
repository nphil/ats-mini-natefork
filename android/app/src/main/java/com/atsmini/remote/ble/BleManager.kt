package com.atsmini.remote.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import com.atsmini.remote.data.LogType
import com.atsmini.remote.data.RadioRepository
import com.atsmini.remote.data.Transport
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.UUID
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Nordic UART BLE transport for the ATS-Mini. */
@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {

    companion object {
        val SERVICE: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // write (app -> radio)
        val TX: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // notify (radio -> app)
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val PREFS = "atsmini.ble"
        private const val KEY_LAST = "lastDeviceAddress"
    }

    data class Found(val name: String, val address: String)

    private val main = Handler(Looper.getMainLooper())
    private val manager get() = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter: BluetoothAdapter? get() = manager.adapter
    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    private var gatt: BluetoothGatt? = null
    private var rxChar: BluetoothGattCharacteristic? = null

    // --- OTA support ------------------------------------------------------------
    // Negotiated ATT MTU (captured in onMtuChanged); payload = mtu - 3.
    @Volatile private var negotiatedMtu = 23
    // When set (during a BLE OTA), incoming TX notifications are routed here
    // instead of RadioRepository, so the OTA driver sees ACK/result lines.
    @Volatile var otaRxListener: ((String) -> Unit)? = null
    // Per-write completion handshake for the blocking raw-write path.
    @Volatile private var writeLatch: CountDownLatch? = null
    @Volatile private var writeStatus = BluetoothGatt.GATT_FAILURE

    /** True once connected with the Nordic UART RX characteristic resolved. */
    val isReady: Boolean get() = gatt != null && rxChar != null

    /** Max raw payload per write (ATT MTU minus the 3-byte notify/write header). */
    fun mtuPayload(): Int = maxOf(20, negotiatedMtu - 3)

    private val _scanning = MutableStateFlow(false)
    val scanning: StateFlow<Boolean> = _scanning.asStateFlow()

    private val _devices = MutableStateFlow<List<Found>>(emptyList())
    val devices: StateFlow<List<Found>> = _devices.asStateFlow()

    private val _connectedDevice = MutableStateFlow<Found?>(null)
    val connectedDevice: StateFlow<Found?> = _connectedDevice.asStateFlow()

    private var autoReconnect = true

    fun isEnabled(): Boolean = adapter?.isEnabled == true

    // --- Scanning ---------------------------------------------------------------

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val dev = result.device ?: return
            val name = dev.name ?: result.scanRecord?.deviceName ?: return
            if (_devices.value.none { it.address == dev.address }) {
                _devices.value = _devices.value + Found(name, dev.address)
            }
            // Silent auto-reconnect to the last radio we used.
            if (autoReconnect && dev.address == prefs.getString(KEY_LAST, null)) {
                stopScan()
                connect(dev)
            }
        }
    }

    fun startScan() {
        val scanner = adapter?.bluetoothLeScanner ?: return
        _devices.value = emptyList()
        _scanning.value = true
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE)).build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        scanner.startScan(listOf(filter), settings, scanCallback)
        RadioRepository.log("Scanning for ATS-Mini…")
        main.postDelayed({ stopScan() }, 15_000)
    }

    fun stopScan() {
        if (!_scanning.value) return
        runCatching { adapter?.bluetoothLeScanner?.stopScan(scanCallback) }
        _scanning.value = false
    }

    // --- Connect ----------------------------------------------------------------

    fun connect(address: String) {
        val dev = adapter?.getRemoteDevice(address) ?: return
        connect(dev)
    }

    private fun connect(device: BluetoothDevice) {
        stopScan()
        autoReconnect = true
        // Close any existing GATT client before opening a new one. Without this,
        // tapping Connect while already connected (or auto-reconnect racing a manual
        // tap) registers a second GATT client on the same link and leaves both alive,
        // causing duplicate notifications and preventing clean command dispatch.
        gatt?.close()
        gatt = null
        rxChar = null
        RadioRepository.log("Connecting to ${device.name ?: device.address}…")
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    fun connectLast(): Boolean {
        val last = prefs.getString(KEY_LAST, null) ?: return false
        connect(last)
        return true
    }

    fun disconnect() {
        autoReconnect = false
        prefs.edit().remove(KEY_LAST).apply()
        gatt?.disconnect()
    }

    fun send(line: String) {
        val g = gatt ?: return
        val c = rxChar ?: return
        val bytes = (line + "\n").toByteArray()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(c, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION")
            c.value = bytes
            @Suppress("DEPRECATION")
            c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            @Suppress("DEPRECATION")
            g.writeCharacteristic(c)
        }
    }

    /**
     * Write one chunk to RX and block until the stack confirms it (write-with-
     * response → onCharacteristicWrite). One outstanding write at a time: this is
     * the back-pressure that keeps a multi-MB OTA stream from overrunning the
     * link. Caller must chunk to [mtuPayload]. Returns true on confirmed success.
     * MUST be called off the main/Binder thread (it blocks).
     */
    fun writeChunkBlocking(bytes: ByteArray, timeoutMs: Long): Boolean {
        val g = gatt ?: return false
        val c = rxChar ?: return false
        val latch = CountDownLatch(1)
        writeLatch = latch
        writeStatus = BluetoothGatt.GATT_FAILURE
        val started = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(c, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) ==
                android.bluetooth.BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION") run {
                c.value = bytes
                c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                g.writeCharacteristic(c)
            }
        }
        if (!started) { writeLatch = null; return false }
        val done = latch.await(timeoutMs, TimeUnit.MILLISECONDS)
        writeLatch = null
        return done && writeStatus == BluetoothGatt.GATT_SUCCESS
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                prefs.edit().putString(KEY_LAST, g.device.address).apply()
                g.requestMtu(517)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                rxChar = null
                gatt = null
                _connectedDevice.value = null
                RadioRepository.detachTransport()
                if (autoReconnect) main.postDelayed({ startScan() }, 1_500)
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            negotiatedMtu = mtu
            g.discoverServices()
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            writeStatus = status
            writeLatch?.countDown()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val service = g.getService(SERVICE) ?: run {
                RadioRepository.log("Nordic UART service not found", LogType.ERROR); return
            }
            rxChar = service.getCharacteristic(RX)
            val txChar = service.getCharacteristic(TX) ?: return
            val devName = g.device.name ?: g.device.address
            _connectedDevice.value = Found(devName, g.device.address)
            g.setCharacteristicNotification(txChar, true)
            val cccd = txChar.getDescriptor(CCCD)
            if (cccd != null) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                } else {
                    @Suppress("DEPRECATION")
                    cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    @Suppress("DEPRECATION")
                    g.writeDescriptor(cccd)
                }
            }
            RadioRepository.attachTransport(Transport.BLE) { line -> send(line) }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION")
            handleRx(c.uuid, c.value)
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray,
        ) {
            handleRx(c.uuid, value)
        }
    }

    private fun handleRx(uuid: UUID, value: ByteArray?) {
        if (uuid != TX || value == null) return
        val text = String(value)
        // During an OTA the driver takes over RX so it sees ACK/result lines;
        // otherwise feed the normal status/command parser.
        val listener = otaRxListener
        if (listener != null) listener(text) else RadioRepository.ingest(text)
    }
}
