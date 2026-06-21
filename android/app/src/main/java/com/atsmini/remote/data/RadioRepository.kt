package com.atsmini.remote.data

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import org.json.JSONObject

/**
 * Single source of truth for radio state, shared by the UI, the foreground
 * service, the home-screen widget and the Quick Settings tile. Transports
 * (BLE / USB) register a [sender] and push received text in via [ingest].
 */
object RadioRepository {

    private val _status = MutableStateFlow(RadioStatus())
    val status: StateFlow<RadioStatus> = _status.asStateFlow()

    private val _logs = MutableStateFlow<List<LogEntry>>(emptyList())
    val logs: StateFlow<List<LogEntry>> = _logs.asStateFlow()

    private val _scan = MutableStateFlow<ScanData?>(null)
    val scan: StateFlow<ScanData?> = _scan.asStateFlow()

    private val _scanProgress = MutableStateFlow(0.0)
    val scanProgress: StateFlow<Double> = _scanProgress.asStateFlow()

    private val _waterfall = MutableStateFlow<List<IntArray>>(emptyList())
    val waterfall: StateFlow<List<IntArray>> = _waterfall.asStateFlow()

    private val _waterfallMeta = MutableStateFlow<WaterfallMeta?>(null)
    val waterfallMeta: StateFlow<WaterfallMeta?> = _waterfallMeta.asStateFlow()

    private val _presets = MutableStateFlow<List<Preset>>(emptyList())
    val presets: StateFlow<List<Preset>> = _presets.asStateFlow()

    /** Raw console text (USB serial console / boot+panic log capture). */
    private val _console = MutableStateFlow("")
    val console: StateFlow<String> = _console.asStateFlow()

    @Volatile private var sender: ((String) -> Unit)? = null
    private val rxBuffer = StringBuilder()

    // --- Transport registration -------------------------------------------------

    fun attachTransport(transport: Transport, send: (String) -> Unit) {
        sender = send
        _status.update { it.copy(transport = transport, connectionStatus = "Connected") }
        log("Connected via $transport", LogType.OK)
        // Bring up the live status stream + presets on connect.
        send(Protocol.subscribe(250)) // floor interval; firmware sends only on change or 2 s keepalive
        send(Protocol.listPresets())
        send(Protocol.setTimeNow())
    }

    fun detachTransport() {
        sender = null
        rxBuffer.setLength(0)
        _status.update { it.copy(transport = Transport.NONE, connectionStatus = "Disconnected") }
        log("Disconnected")
    }

    fun send(json: String) {
        sender?.invoke(json) ?: log("Not connected", LogType.ERROR)
    }

    val isConnected: Boolean get() = sender != null

    // --- Console (USB raw bytes) ------------------------------------------------

    fun appendConsole(text: String) {
        _console.update { (it + text).takeLast(20_000) }
    }

    fun clearConsole() { _console.value = "" }

    // --- Inbound line protocol --------------------------------------------------

    /** Feed received characters; complete `{...}` objects are parsed out. */
    fun ingest(chunk: String) {
        rxBuffer.append(chunk)
        var text = rxBuffer.toString()
        while (true) {
            val start = text.indexOf('{')
            if (start < 0) { text = ""; break }
            var depth = 0
            var end = -1
            var i = start
            while (i < text.length) {
                when (text[i]) {
                    '{' -> depth++
                    '}' -> { depth--; if (depth == 0) { end = i; break } }
                }
                i++
            }
            if (end < 0) { text = text.substring(start); break }
            val json = text.substring(start, end + 1)
            text = text.substring(end + 1)
            runCatching { handle(JSONObject(json)) }
        }
        rxBuffer.setLength(0)
        rxBuffer.append(text)
    }

    private fun handle(msg: JSONObject) {
        when (msg.optString("t")) {
            "s" -> _status.update { RadioStatus.applyStatus(it, msg) }
            "scan" -> handleScan(msg)
            "scan_prog" -> _scanProgress.value = msg.optDouble("pct", 0.0)
            "wf" -> handleWaterfall(msg)
            "wf_done" -> log("Waterfall session ended")
            "presets" -> handlePresets(msg)
            "preset_saved" -> { log("Preset saved", LogType.OK); send(Protocol.listPresets()) }
            "preset_deleted", "preset_renamed" -> send(Protocol.listPresets())
            "err" -> log("Device error: ${msg.optString("msg", "unknown")}", LogType.ERROR)
        }
    }

    private fun handleScan(msg: JSONObject) {
        val r = msg.optJSONArray("r") ?: return
        val rssi = IntArray(r.length()) { r.optInt(it) }.toList()
        val snArr = msg.optJSONArray("sn")
        val snr = if (snArr != null) IntArray(snArr.length()) { snArr.optInt(it) }.toList() else emptyList()
        val chArr = msg.optJSONArray("ch")
        val ch = if (chArr != null) IntArray(chArr.length()) { chArr.optInt(it) }.toList() else emptyList()
        _scan.value = ScanData(
            startFreq = msg.optInt("sf"), step = msg.optInt("step"),
            pointCount = msg.optInt("n"), rssi = rssi, snr = snr, channels = ch,
        )
        _scanProgress.value = 0.0
        log("Scan complete: ${rssi.size} points")
    }

    private fun handleWaterfall(msg: JSONObject) {
        val r = msg.optJSONArray("r") ?: return
        val row = IntArray(r.length()) { r.optInt(it).coerceIn(0, 255) }
        if (msg.has("sf")) {
            _waterfallMeta.value = WaterfallMeta(msg.optInt("sf"), msg.optInt("step"), msg.optInt("n"))
        }
        _waterfall.update { (it + row).takeLast(200) }
    }

    private fun handlePresets(msg: JSONObject) {
        val list = msg.optJSONArray("list") ?: return
        val presets = ArrayList<Preset>(list.length())
        for (i in 0 until list.length()) {
            val o = list.optJSONObject(i) ?: continue
            presets.add(Preset(o.optInt("idx"), o.optString("name"), o.optInt("ch")))
        }
        _presets.value = presets
    }

    fun clearWaterfall() { _waterfall.value = emptyList() }

    // --- Logging ----------------------------------------------------------------

    fun log(message: String, type: LogType = LogType.INFO) {
        _logs.update { (it + LogEntry(System.currentTimeMillis(), message, type)).takeLast(200) }
    }

    fun clearLogs() { _logs.value = emptyList() }
}
