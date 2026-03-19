import SwiftUI

struct FrequencyCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var quickTuneText = ""
    @State private var showQuickTune = false

    var body: some View {
        GlassCard {
            VStack(spacing: 12) {
                // Frequency display
                HStack(spacing: 0) {
                    // Seek down
                    Button { ble.sendSeek(-1) } label: {
                        Image(systemName: "backward.end.fill")
                            .font(.title3)
                            .frame(width: 44, height: 44)
                    }
                    .foregroundStyle(.cyan)
                    .disabled(!radio.isConnected || radio.isSeeking)

                    // Step down
                    Button { ble.sendRaw("r") } label: {
                        Image(systemName: "chevron.left")
                            .font(.title2)
                            .frame(width: 44, height: 44)
                    }
                    .foregroundStyle(Color(red: 0, green: 1, blue: 0.25))
                    .disabled(!radio.isConnected)

                    // Main frequency
                    VStack(spacing: 4) {
                        Text(radio.formattedFrequency)
                            .font(.system(size: 48, weight: .bold, design: .monospaced))
                            .foregroundStyle(Color(red: 0, green: 1, blue: 0.25))
                            .shadow(color: Color(red: 0, green: 1, blue: 0.25).opacity(0.4), radius: 10)
                            .contentTransition(.numericText())
                            .onTapGesture { showQuickTune.toggle() }

                        // Unit selector
                        HStack(spacing: 12) {
                            ForEach(FreqUnit.allCases, id: \.self) { unit in
                                Text(unit.rawValue)
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundStyle(radio.freqDisplayUnit == unit ?
                                        Color(red: 0, green: 0.8, blue: 0.27) : Color(red: 0, green: 0.35, blue: 0.08))
                                    .onTapGesture { radio.freqDisplayUnit = unit }
                            }
                        }
                    }
                    .frame(maxWidth: .infinity)

                    // Step up
                    Button { ble.sendRaw("R") } label: {
                        Image(systemName: "chevron.right")
                            .font(.title2)
                            .frame(width: 44, height: 44)
                    }
                    .foregroundStyle(Color(red: 0, green: 1, blue: 0.25))
                    .disabled(!radio.isConnected)

                    // Seek up
                    Button { ble.sendSeek(1) } label: {
                        Image(systemName: "forward.end.fill")
                            .font(.title3)
                            .frame(width: 44, height: 44)
                    }
                    .foregroundStyle(.cyan)
                    .disabled(!radio.isConnected || radio.isSeeking)
                }
                .padding(.vertical, 8)
                .background(Color(red: 0.04, green: 0.1, blue: 0.04))
                .clipShape(RoundedRectangle(cornerRadius: 8))

                if radio.isSeeking {
                    Text("SEEKING...")
                        .font(.system(.caption, design: .monospaced))
                        .foregroundStyle(.cyan)
                }

                // Mode info
                Text("BAND: \(radio.bandName)  MODE: \(radio.modeName)  STEP: \(radio.stepSize)  BW: \(radio.bandwidth)")
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(.secondary)

                // Quick tune
                if showQuickTune {
                    HStack {
                        TextField("e.g. 98.5 MHz, 7125 kHz", text: $quickTuneText)
                            .font(.system(.body, design: .monospaced))
                            .textFieldStyle(.roundedBorder)
                            .keyboardType(.decimalPad)
                            .onSubmit { commitQuickTune() }
                        Button("Tune") { commitQuickTune() }
                            .font(.caption.bold())
                            .foregroundStyle(Color.accent)
                    }
                }
            }
        }
    }

    private func commitQuickTune() {
        guard let freq = parseQuickTune(quickTuneText) else { return }
        ble.sendFrequency(freq)
        quickTuneText = ""
        showQuickTune = false
    }

    private func parseQuickTune(_ s: String) -> Int? {
        let cleaned = s.trimmingCharacters(in: .whitespaces).replacingOccurrences(of: ",", with: "")
        guard !cleaned.isEmpty else { return nil }
        let lower = cleaned.lowercased()
        let hasMHz = lower.contains("mhz") || lower.hasSuffix("m")
        let hasKHz = lower.contains("khz") || lower.hasSuffix("k")
        guard let v = Double(cleaned.filter { $0.isNumber || $0 == "." }), v > 0 else { return nil }
        let inMHz = hasMHz || (!hasKHz && v < 200)
        if radio.isFM {
            return inMHz ? Int(round(v * 100)) : Int(round(v / 10))
        } else {
            return inMHz ? Int(round(v * 1000)) : Int(round(v))
        }
    }
}
