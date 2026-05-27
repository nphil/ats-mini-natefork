import Foundation
import CoreBluetooth
import Combine

/// Manages BLE connection to ATS-Mini using Nordic UART Service
final class BLEManager: NSObject, ObservableObject {
    static let shared = BLEManager()

    // Nordic UART Service UUIDs
    private let serviceUUID = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    private let txUUID      = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // notify (radio → app)
    private let rxUUID      = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // write  (app → radio)

    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?
    private var txCharacteristic: CBCharacteristic?

    private var rxBuffer = ""

    @Published var isScanning = false
    @Published var discoveredDevices: [CBPeripheral] = []

    /// Set before calling startScan() to auto-connect to the first device with this name.
    var autoConnectName: String?

    /// Name of the currently (or last) connected peripheral — used for auto-reconnect after OTA.
    private(set) var connectedPeripheralName: String?

    weak var radio: RadioState?

    // MARK: - Auto-reconnect

    /// CoreBluetooth peripheral UUID of the last successfully-connected radio.
    /// Persisted so the app can quietly reconnect every time the radio comes
    /// back online (e.g. after the firmware's 5-min BLE auto-off + new boot).
    private static let lastPeripheralKey = "atsmini.ble.lastPeripheralUUID"

    private var lastPeripheralUUID: UUID? {
        get {
            UserDefaults.standard.string(forKey: Self.lastPeripheralKey).flatMap(UUID.init)
        }
        set {
            UserDefaults.standard.set(newValue?.uuidString, forKey: Self.lastPeripheralKey)
        }
    }

    /// When true, scan results matching the persisted peripheral UUID auto-connect.
    /// Reset when the user explicitly disconnects so we don't bounce back on them.
    private var autoReconnectEnabled = true

    private override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    // MARK: - Auto-reconnect entry points

    /// Called once at app launch (or whenever Bluetooth becomes available).
    /// If we know the UUID of the last connected radio, try a fast direct
    /// reconnect via CoreBluetooth's known-peripheral cache; otherwise fall
    /// back to a passive scan that picks the radio up when it advertises.
    func beginAutoReconnect() {
        guard autoReconnectEnabled,
              centralManager.state == .poweredOn,
              !isConnected else { return }

        if let saved = lastPeripheralUUID,
           let known = centralManager.retrievePeripherals(withIdentifiers: [saved]).first {
            radio?.log("Auto-reconnecting to last ATS-Mini…")
            connect(to: known, isAuto: true)
            return
        }

        // No cached peripheral handle — passive scan so we connect as soon as
        // the radio re-advertises. This is silent in the UI: no "Scanning…"
        // label, no auto-stop timeout, no user prompt.
        startBackgroundScan()
    }

    private func startBackgroundScan() {
        guard centralManager.state == .poweredOn, !isScanning else { return }
        isScanning = true
        centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
    }

    func startScan() {
        guard centralManager.state == .poweredOn else {
            radio?.log("Bluetooth not ready", type: .error)
            return
        }
        discoveredDevices.removeAll()
        isScanning = true
        centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
        radio?.log("Scanning for ATS-Mini...")
        radio?.connectionStatus = "Scanning..."

        // Auto-stop scan after 15s
        DispatchQueue.main.asyncAfter(deadline: .now() + 15) { [weak self] in
            if self?.isScanning == true {
                self?.stopScan()
            }
        }
    }

    func stopScan() {
        centralManager.stopScan()
        isScanning = false
    }

    func connect(to peripheral: CBPeripheral) {
        connect(to: peripheral, isAuto: false)
    }

    private func connect(to peripheral: CBPeripheral, isAuto: Bool) {
        stopScan()
        self.peripheral = peripheral
        peripheral.delegate = self
        radio?.connectionStatus = isAuto ? "Reconnecting…" : "Connecting..."
        // Any user-initiated connect re-enables auto-reconnect for next time
        if !isAuto { autoReconnectEnabled = true }
        centralManager.connect(peripheral, options: nil)
    }

    func disconnect() {
        // Explicit user disconnect: forget the saved peripheral so we don't
        // immediately reconnect to a radio they just deliberately released.
        autoReconnectEnabled = false
        lastPeripheralUUID = nil
        if let p = peripheral {
            centralManager.cancelPeripheralConnection(p)
        }
    }

    var isConnected: Bool {
        peripheral?.state == .connected && rxCharacteristic != nil
    }

    // MARK: - Send

    func send(_ command: [String: Any]) {
        guard let rx = rxCharacteristic, let p = peripheral else { return }
        guard let data = try? JSONSerialization.data(withJSONObject: command) else { return }
        p.writeValue(data, for: rx, type: .withResponse)
    }

    func sendRaw(_ string: String) {
        guard let rx = rxCharacteristic, let p = peripheral else { return }
        guard let data = string.data(using: .utf8) else { return }
        p.writeValue(data, for: rx, type: .withResponse)
    }

    // MARK: - Convenience commands

    func sendDelta(_ cmd: String, delta: Int) {
        send(["cmd": cmd, "d": delta])
    }

    func sendFrequency(_ freq: Int) {
        send(["cmd": "freq", "val": freq])
    }

    func sendSeek(_ direction: Int) {
        radio?.isSeeking = true
        send(["cmd": "seek", "d": direction])
    }

    func sendVolumeDelta(_ delta: Int) {
        send(["cmd": "vol", "d": delta])
    }

    func sendClick() {
        send(["cmd": "click"])
    }

    func sendSleep(_ on: Bool) {
        send(["cmd": "sleep", "on": on ? 1 : 0])
    }

    func sendScan(step: Int) {
        radio?.isScanning = true
        radio?.scanProgress = 0
        send(["cmd": "scan", "step": step])
    }

    func sendWaterfallStart(sf: Int? = nil, step: Int? = nil, n: Int? = nil) {
        var cmd: [String: Any] = ["cmd": "wf_start"]
        if let sf = sf { cmd["sf"] = sf }
        if let step = step { cmd["step"] = step }
        if let n = n { cmd["n"] = n }
        send(cmd)
        radio?.isWaterfallActive = true
    }

    func sendWaterfallStop() {
        send(["cmd": "wf_stop"])
        radio?.isWaterfallActive = false
    }

    func sendSavePreset(name: String) {
        send(["cmd": "save_preset", "name": name])
    }

    func sendLoadPreset(idx: Int) {
        send(["cmd": "load_preset", "idx": idx])
    }

    func sendDeletePreset(idx: Int) {
        send(["cmd": "delete_preset", "idx": idx])
    }

    func sendRenamePreset(idx: Int, name: String) {
        send(["cmd": "rename_preset", "idx": idx, "name": name])
    }

    func sendListPresets() {
        send(["cmd": "list_presets"])
    }

    func syncTime() {
        let now = Date()
        let cal = Calendar.current
        let hh = cal.component(.hour, from: now)
        let mm = cal.component(.minute, from: now)
        let ss = cal.component(.second, from: now)
        send(["cmd": "settime", "hh": hh, "mm": mm, "ss": ss])
        radio?.log("Time sync -> \(String(format: "%02d:%02d:%02d", hh, mm, ss))", type: .ok)
    }

    // MARK: - Message parsing

    private func processBuffer() {
        while let startIdx = rxBuffer.firstIndex(of: "{") {
            var depth = 0
            var endIdx: String.Index?
            var idx = startIdx
            while idx < rxBuffer.endIndex {
                if rxBuffer[idx] == "{" { depth += 1 }
                else if rxBuffer[idx] == "}" {
                    depth -= 1
                    if depth == 0 { endIdx = idx; break }
                }
                idx = rxBuffer.index(after: idx)
            }
            guard let end = endIdx else { break }
            let jsonStr = String(rxBuffer[startIdx...end])
            rxBuffer = String(rxBuffer[rxBuffer.index(after: end)...])

            if let data = jsonStr.data(using: .utf8),
               let msg = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
                handleMessage(msg)
            }
        }
        // Discard garbage before first {
        if let first = rxBuffer.firstIndex(of: "{"), first != rxBuffer.startIndex {
            rxBuffer = String(rxBuffer[first...])
        } else if !rxBuffer.contains("{") {
            rxBuffer = ""
        }
    }

    private func handleMessage(_ msg: [String: Any]) {
        guard let type = msg["t"] as? String else { return }

        switch type {
        case "s":
            radio?.updateFromStatus(msg)

        case "scan":
            handleScan(msg)

        case "scan_prog":
            if let pct = msg["pct"] as? Double {
                DispatchQueue.main.async { self.radio?.scanProgress = pct }
            }

        case "wf":
            handleWaterfallRow(msg)

        case "wf_done":
            DispatchQueue.main.async {
                self.radio?.isWaterfallActive = false
                self.radio?.log("Waterfall session ended")
            }

        case "ack":
            if let cmd = msg["cmd"] as? String, cmd == "wf_start" {
                // Store waterfall meta from ack
                if let sf = msg["sf"] as? Int, let step = msg["step"] as? Int, let n = msg["n"] as? Int {
                    DispatchQueue.main.async {
                        self.radio?.waterfallMeta = WFMeta(startFreq: sf, step: step, pointCount: n)
                    }
                }
            }

        case "presets":
            handlePresets(msg)

        case "preset_saved":
            radio?.log("Preset saved: \(msg["name"] as? String ?? "")", type: .ok)
            sendListPresets()

        case "preset_deleted", "preset_renamed":
            sendListPresets()

        case "preset_loaded":
            if let ch = msg["ch"] as? [Int], let name = msg["name"] as? String {
                DispatchQueue.main.async {
                    if var scan = self.radio?.scanData {
                        scan.channels = ch
                        self.radio?.scanData = scan
                    }
                }
                radio?.log("Loaded preset \"\(name)\": \(ch.count) channels")
            }

        case "err":
            let errMsg = msg["msg"] as? String ?? "Unknown error"
            radio?.log("Device error: \(errMsg)", type: .error)

        default:
            break
        }
    }

    private func handleScan(_ msg: [String: Any]) {
        guard let sf = msg["sf"] as? Int,
              let step = msg["step"] as? Int,
              let n = msg["n"] as? Int,
              let rssi = msg["r"] as? [Int] else { return }
        let snr = msg["sn"] as? [Int] ?? []
        let ch = msg["ch"] as? [Int] ?? []

        DispatchQueue.main.async {
            self.radio?.scanData = ScanData(startFreq: sf, step: step, pointCount: n, rssi: rssi, snr: snr, channels: ch)
            self.radio?.isScanning = false
            self.radio?.scanProgress = 0
            self.radio?.log("Scan complete: \(n) points")
        }
    }

    private func handleWaterfallRow(_ msg: [String: Any]) {
        guard let rssiArr = msg["r"] as? [Int] else { return }
        let row = rssiArr.map { UInt8(min(255, max(0, $0))) }

        if let sf = msg["sf"] as? Int, let step = msg["step"] as? Int, let n = msg["n"] as? Int {
            DispatchQueue.main.async {
                self.radio?.waterfallMeta = WFMeta(startFreq: sf, step: step, pointCount: n)
            }
        }

        DispatchQueue.main.async {
            self.radio?.waterfallRows.append(row)
            if self.radio?.waterfallRows.count ?? 0 > 200 {
                self.radio?.waterfallRows.removeFirst()
            }
        }
    }

    private func handlePresets(_ msg: [String: Any]) {
        guard let list = msg["list"] as? [[String: Any]] else { return }
        let presets = list.compactMap { item -> Preset? in
            guard let idx = item["idx"] as? Int,
                  let name = item["name"] as? String,
                  let ch = item["ch"] as? Int else { return nil }
            return Preset(idx: idx, name: name, channelCount: ch)
        }
        DispatchQueue.main.async {
            self.radio?.presets = presets
        }
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            // Bluetooth just came up — try the silent reconnect path now.
            beginAutoReconnect()
        } else {
            radio?.log("Bluetooth: \(central.state.rawValue)", type: .error)
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                         advertisementData: [String: Any], rssi RSSI: NSNumber) {
        if !discoveredDevices.contains(where: { $0.identifier == peripheral.identifier }) {
            discoveredDevices.append(peripheral)
            radio?.log("Found: \(peripheral.name ?? peripheral.identifier.uuidString)")
        }

        // Path 1: auto-reconnect by remembered UUID (silent — fires whenever
        // the last-known radio re-advertises, e.g. after its 5-min BLE
        // auto-off cycle or a power-on).
        if autoReconnectEnabled,
           let saved = lastPeripheralUUID,
           peripheral.identifier == saved {
            connect(to: peripheral, isAuto: true)
            return
        }

        // Path 2: auto-connect by name after an OTA reboot (existing behavior).
        if let target = autoConnectName, peripheral.name == target {
            autoConnectName = nil
            connect(to: peripheral)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectedPeripheralName = peripheral.name
        // Remember this peripheral for silent reconnect on next launch / next
        // time the radio comes online.
        lastPeripheralUUID = peripheral.identifier
        autoReconnectEnabled = true
        radio?.log("Connected to \(peripheral.name ?? "device")", type: .ok)
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        DispatchQueue.main.async {
            self.radio?.isConnected = false
            self.radio?.connectionStatus = "Failed"
        }
        radio?.log("Connection failed: \(error?.localizedDescription ?? "unknown")", type: .error)

        // Failed during a silent reconnect attempt → fall back to a passive
        // background scan so we still pick the radio up when it advertises.
        if autoReconnectEnabled { startBackgroundScan() }
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        DispatchQueue.main.async {
            self.radio?.isConnected = false
            self.radio?.connectionStatus = "Disconnected"
        }
        rxCharacteristic = nil
        txCharacteristic = nil
        radio?.log("Disconnected")

        // Involuntary disconnect (firmware sleep, out of range, BLE auto-off):
        // immediately start a quiet background scan so we reconnect the moment
        // the radio reappears. User-initiated disconnect() turned this off
        // already, so we won't fight them.
        if autoReconnectEnabled { startBackgroundScan() }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services where service.uuid == serviceUUID {
            peripheral.discoverCharacteristics([txUUID, rxUUID], for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let chars = service.characteristics else { return }
        for char in chars {
            if char.uuid == txUUID {
                txCharacteristic = char
                peripheral.setNotifyValue(true, for: char)
            } else if char.uuid == rxUUID {
                rxCharacteristic = char
            }
        }
        if rxCharacteristic != nil && txCharacteristic != nil {
            DispatchQueue.main.async {
                self.radio?.isConnected = true
                self.radio?.connectionStatus = "Connected"
            }
            // Subscribe to status updates
            send(["cmd": "sub", "ms": 500])
            // Sync time
            syncTime()
            // Load presets
            sendListPresets()
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == txUUID, let data = characteristic.value else { return }
        guard let chunk = String(data: data, encoding: .utf8) else { return }

        // Log non-status messages
        if !chunk.trimmingCharacters(in: .whitespaces).hasPrefix("{\"t\":\"s\"") {
            let preview = String(chunk.prefix(80)).replacingOccurrences(of: "\n", with: " ")
            radio?.log("BLE rx (\(chunk.count) bytes): \(preview)")
        }

        rxBuffer += chunk
        processBuffer()
    }
}
