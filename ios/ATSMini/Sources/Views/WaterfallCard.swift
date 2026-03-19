import SwiftUI

struct WaterfallCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var startFreqText = ""
    @State private var endFreqText = ""

    var body: some View {
        GlassCard {
            VStack(spacing: 10) {
                // Header
                HStack {
                    Text("WATERFALL")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .tracking(1)
                    Spacer()
                    if let meta = radio.waterfallMeta {
                        let endF = meta.startFreq + meta.step * (meta.pointCount - 1)
                        Text("n=\(meta.pointCount) \(fmtFreq(meta.startFreq))-\(fmtFreq(endF))")
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                }

                // Controls
                HStack(spacing: 8) {
                    if radio.isWaterfallActive {
                        Button {
                            ble.sendWaterfallStop()
                        } label: {
                            Label("Stop", systemImage: "stop.fill")
                                .font(.caption)
                        }
                        .buttonStyle(.bordered)
                        .tint(.red)
                    } else {
                        Button {
                            radio.waterfallRows.removeAll()
                            ble.sendWaterfallStart()
                        } label: {
                            Label("Start", systemImage: "play.fill")
                                .font(.caption)
                        }
                        .buttonStyle(.bordered)
                        .tint(.green)
                        .disabled(!radio.isConnected)
                    }

                    // Zoom
                    Button {
                        if radio.waterfallZoom > 0 { radio.waterfallZoom -= 1 }
                    } label: {
                        Image(systemName: "minus.magnifyingglass")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                    .disabled(radio.waterfallZoom <= 0)

                    Text("x\(1 << radio.waterfallZoom)")
                        .font(.system(.caption, design: .monospaced))
                        .frame(width: 30)

                    Button {
                        if radio.waterfallZoom < 5 { radio.waterfallZoom += 1 }
                    } label: {
                        Image(systemName: "plus.magnifyingglass")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                    .disabled(radio.waterfallZoom >= 5)
                }

                // Band/Mode controls
                HStack(spacing: 8) {
                    Text("Band")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Button { ble.sendDelta("band", delta: -1) } label: {
                        Image(systemName: "chevron.left").font(.caption2)
                    }
                    .buttonStyle(.bordered)
                    Text(radio.bandName)
                        .font(.system(.caption, design: .monospaced))
                    Button { ble.sendDelta("band", delta: 1) } label: {
                        Image(systemName: "chevron.right").font(.caption2)
                    }
                    .buttonStyle(.bordered)

                    Spacer()

                    Text("Mode")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Button { ble.sendDelta("mode", delta: -1) } label: {
                        Image(systemName: "chevron.left").font(.caption2)
                    }
                    .buttonStyle(.bordered)
                    Text(radio.modeName)
                        .font(.system(.caption, design: .monospaced))
                    Button { ble.sendDelta("mode", delta: 1) } label: {
                        Image(systemName: "chevron.right").font(.caption2)
                    }
                    .buttonStyle(.bordered)
                }

                // Freq range inputs
                HStack(spacing: 8) {
                    Text("Start")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    TextField(radio.isFM ? "87.5" : "530", text: $startFreqText)
                        .font(.system(.caption, design: .monospaced))
                        .textFieldStyle(.roundedBorder)
                        .keyboardType(.decimalPad)
                        .frame(width: 80)

                    Text("End")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    TextField(radio.isFM ? "108.0" : "1700", text: $endFreqText)
                        .font(.system(.caption, design: .monospaced))
                        .textFieldStyle(.roundedBorder)
                        .keyboardType(.decimalPad)
                        .frame(width: 80)

                    Text(radio.isFM ? "MHz" : "kHz")
                        .font(.caption2)
                        .foregroundStyle(.secondary)

                    Button("Apply") {
                        applyRange()
                    }
                    .font(.caption)
                    .buttonStyle(.bordered)
                    .tint(.accent)

                    Button("Reset") {
                        radio.waterfallRows.removeAll()
                        radio.waterfallMeta = nil
                        startFreqText = ""
                        endFreqText = ""
                    }
                    .font(.caption)
                    .buttonStyle(.bordered)
                }

                // Waterfall canvas
                WaterfallCanvas()
                    .frame(height: 200)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
            }
        }
    }

    private func fmtFreq(_ f: Int) -> String {
        radio.isFM ? String(format: "%.2f MHz", Double(f) / 100.0) : "\(f) kHz"
    }

    private func applyRange() {
        guard let sv = Double(startFreqText), let ev = Double(endFreqText), ev > sv else { return }
        let sf = radio.isFM ? Int(round(sv * 100)) : Int(round(sv))
        let end = radio.isFM ? Int(round(ev * 100)) : Int(round(ev))
        let step = max(1, (end - sf) / 100)
        let n = min(200, max(4, (end - sf) / step + 1))
        radio.waterfallRows.removeAll()
        ble.sendWaterfallStart(sf: sf, step: step, n: n)
    }
}

struct WaterfallCanvas: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        Canvas { context, size in
            let W = size.width
            let H = size.height

            // Background
            context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(.black))

            let rows = radio.waterfallRows
            guard !rows.isEmpty else { return }

            let maxRows = Int(H)
            let displayRows = rows.suffix(maxRows)

            for (rowIdx, row) in displayRows.enumerated() {
                let y = H - Double(displayRows.count - rowIdx)
                if y < 0 { continue }
                let n = row.count
                guard n > 0 else { continue }

                for px in 0..<Int(W) {
                    let di = min(n - 1, px * (n - 1) / max(1, Int(W) - 1))
                    let rssi = Double(row[di])
                    let color = heatColor(rssi: rssi)
                    let rect = CGRect(x: Double(px), y: y, width: 1, height: 1)
                    context.fill(Path(rect), with: .color(color))
                }
            }
        }
    }

    private func heatColor(rssi: Double) -> Color {
        let t = min(1, max(0, rssi / 80.0))
        // HSV: hue from 240 (blue) to 0 (red)
        let hue = (1 - t) * 240.0 / 360.0
        let saturation = 1.0
        let brightness = 0.10 + 0.90 * t
        return Color(hue: hue, saturation: saturation, brightness: brightness)
    }
}
