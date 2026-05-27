import Foundation
import Combine

/// Observable model holding all radio state received via BLE status packets
final class RadioState: ObservableObject {
    // Connection
    @Published var isConnected = false
    @Published var connectionStatus = "Disconnected"

    // Frequency & tuning
    @Published var frequency: Int = 0          // internal units (FM: ×10kHz, AM/SSB: kHz)
    @Published var bfo: Int = 0
    @Published var mode: Int = 0               // 0=FM, 1=LSB, 2=USB, 3=AM
    @Published var modeName = "---"
    @Published var bandName = "---"
    @Published var stepSize = "---"
    @Published var bandwidth = "---"
    @Published var agc = "--"

    // Signal
    @Published var rssi: Int = 0               // 0-127
    @Published var snr: Int = 0                // 0-30+
    @Published var cpu0: Int = 0
    @Published var cpu1: Int = 0

    // Battery
    @Published var batteryVoltage: Double = 0.0

    // Volume
    @Published var volume: Int = 35

    // RDS (FM only)
    @Published var rdsStation = ""
    @Published var rdsText = ""
    @Published var rdsPTY = ""
    @Published var rdsTime = ""

    // Scan
    @Published var scanData: ScanData?
    @Published var scanProgress: Double = 0
    @Published var isScanning = false

    // Waterfall
    @Published var waterfallRows: [[UInt8]] = []
    @Published var waterfallMeta: WFMeta?
    @Published var isWaterfallActive = false
    @Published var waterfallZoom: Int = 0

    // Presets
    @Published var presets: [Preset] = []

    // Sequence
    @Published var sequenceNumber: Int = 0

    // Log
    @Published var logMessages: [LogEntry] = []

    // Seek
    @Published var isSeeking = false

    // WiFi / OTA
    @Published var wifiIP = ""        // device IP for HTTP OTA (STA or AP)
    @Published var wifiIsAP = false   // true when device is in AP mode
    @Published var firmwareVersion: Int = 0  // e.g. 235 for v2.35

    // RDS PS stability filter
    private var psStableValue = ""
    private var psPendingValue = ""
    private var psPendingCount = 0
    private var psLastFreq: Int = -1

    // Frequency display unit
    @Published var freqDisplayUnit: FreqUnit = .auto

    var isFM: Bool { mode == 0 }

    var batteryPercent: Double {
        max(0, min(100, (batteryVoltage - 3.3) / (4.2 - 3.3) * 100))
    }

    var formattedFrequency: String {
        let unit = resolvedUnit
        if unit == .mhz {
            let mhz = isFM ? Double(frequency) / 100.0 : Double(frequency) / 1000.0
            return String(format: isFM ? "%.2f" : "%.3f", mhz)
        } else {
            let khz = isFM ? frequency * 10 : frequency
            return "\(khz)"
        }
    }

    var frequencyUnitLabel: String {
        resolvedUnit == .mhz ? "MHz" : "kHz"
    }

    var resolvedUnit: FreqUnit {
        switch freqDisplayUnit {
        case .auto: return isFM ? .mhz : .khz
        case .mhz: return .mhz
        case .khz: return .khz
        }
    }

    func accumulatePS(_ ps: String, freq: Int) -> String {
        if freq != psLastFreq {
            psStableValue = ""
            psPendingValue = ""
            psPendingCount = 0
            psLastFreq = freq
        }
        let chunk = ps.trimmingCharacters(in: .whitespaces)
        if chunk.isEmpty { return psStableValue }
        if chunk == psPendingValue {
            psPendingCount += 1
        } else {
            psPendingValue = chunk
            psPendingCount = 1
        }
        if psPendingCount >= 2 { psStableValue = psPendingValue }
        return psStableValue
    }

    func log(_ message: String, type: LogEntry.LogType = .info) {
        DispatchQueue.main.async {
            let entry = LogEntry(message: message, type: type)
            self.logMessages.append(entry)
            if self.logMessages.count > 120 {
                self.logMessages.removeFirst()
            }
        }
    }

    func updateFromStatus(_ msg: [String: Any]) {
        DispatchQueue.main.async {
            if let f = msg["f"] as? Int { self.frequency = f }
            if let m = msg["m"] as? Int { self.mode = m }
            if let mn = msg["mn"] as? String { self.modeName = mn }
            if let bn = msg["bn"] as? String { self.bandName = bn }
            if let st = msg["st"] as? String { self.stepSize = st }
            if let bw = msg["bw"] as? String { self.bandwidth = bw }
            if let agc = msg["agc"] as? Int { self.agc = "\(agc)" }
            if let r = msg["r"] as? Int { self.rssi = r }
            if let sn = msg["sn"] as? Int { self.snr = sn }
            if let v = msg["v"] as? Int { self.volume = v }
            if let bat = msg["bat"] as? Double { self.batteryVoltage = bat }
            if let seq = msg["seq"] as? Int { self.sequenceNumber = seq }
            if let c0 = msg["cpu0"] as? Int { self.cpu0 = c0 }
            if let c1 = msg["cpu1"] as? Int { self.cpu1 = c1 }
            if let bfo = msg["bfo"] as? Int { self.bfo = bfo }

            // RDS
            if self.mode == 0 {
                if let ps = msg["ps"] as? String {
                    self.rdsStation = self.accumulatePS(ps, freq: self.frequency)
                }
                if let rt = msg["rt"] as? String { self.rdsText = rt.trimmingCharacters(in: .whitespaces) }
                if let pty = msg["pty"] as? String { self.rdsPTY = pty.trimmingCharacters(in: .whitespaces) }
                if let ct = msg["ct"] as? String { self.rdsTime = ct }
            } else {
                self.rdsStation = ""
                self.rdsText = ""
                self.rdsPTY = ""
                self.rdsTime = ""
            }

            // WiFi / OTA info
            if let wip = msg["wip"] as? String { self.wifiIP = wip }
            if let wm  = msg["wm"]  as? Int    { self.wifiIsAP = (wm == 1) }
            if let fw  = msg["fw"]  as? Int    { self.firmwareVersion = fw }

            // Clear seek on new freq
            if self.isSeeking { self.isSeeking = false }
        }
    }
}

enum FreqUnit: String, CaseIterable {
    case auto = "Auto"
    case mhz = "MHz"
    case khz = "kHz"
}

struct LogEntry: Identifiable {
    let id = UUID()
    let timestamp = Date()
    let message: String
    let type: LogType

    enum LogType {
        case info, ok, error
    }

    var color: String {
        switch type {
        case .info: return "muted"
        case .ok: return "green"
        case .error: return "red"
        }
    }
}

struct ScanData {
    let startFreq: Int
    let step: Int
    let pointCount: Int
    let rssi: [Int]
    let snr: [Int]
    var channels: [Int]
}

struct WFMeta {
    let startFreq: Int
    let step: Int
    let pointCount: Int
}

struct Preset: Identifiable {
    let id = UUID()
    let idx: Int
    var name: String
    let channelCount: Int
}
