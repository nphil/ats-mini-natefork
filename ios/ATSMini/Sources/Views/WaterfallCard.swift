import SwiftUI

struct WaterfallCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var startFreqText = ""
    @State private var endFreqText = ""

    var body: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 14) {

                CardHeader(
                    title: "Waterfall",
                    trailing: radio.waterfallMeta.map { meta in
                        let endF = meta.startFreq + meta.step * (meta.pointCount - 1)
                        return "n=\(meta.pointCount)  \(fmtFreq(meta.startFreq))–\(fmtFreq(endF))"
                    }
                )

                // Canvas first — give the data the space
                WaterfallCanvas()
                    .frame(height: 220)
                    .clipShape(.rect(cornerRadius: 14))

                // Start/Stop row
                GlassEffectContainer {
                    HStack(spacing: 10) {
                        if radio.isWaterfallActive {
                            Button {
                                ble.sendWaterfallStop()
                            } label: {
                                Label("Stop", systemImage: "stop.fill")
                                    .frame(maxWidth: .infinity)
                                    .padding(.vertical, 4)
                            }
                            .buttonStyle(.glassProminent)
                            .tint(.red)
                        } else {
                            Button {
                                radio.waterfallRows.removeAll()
                                ble.sendWaterfallStart()
                            } label: {
                                Label("Start", systemImage: "play.fill")
                                    .frame(maxWidth: .infinity)
                                    .padding(.vertical, 4)
                            }
                            .buttonStyle(.glassProminent)
                            .tint(.green)
                            .disabled(!radio.isConnected)
                        }

                        // Zoom group
                        HStack(spacing: 4) {
                            Button {
                                if radio.waterfallZoom > 0 { radio.waterfallZoom -= 1 }
                            } label: {
                                Image(systemName: "minus.magnifyingglass")
                            }
                            .buttonStyle(.glass)
                            .disabled(radio.waterfallZoom <= 0)

                            Text("×\(1 << radio.waterfallZoom)")
                                .font(.caption.monospaced().weight(.medium))
                                .frame(minWidth: 28)

                            Button {
                                if radio.waterfallZoom < 5 { radio.waterfallZoom += 1 }
                            } label: {
                                Image(systemName: "plus.magnifyingglass")
                            }
                            .buttonStyle(.glass)
                            .disabled(radio.waterfallZoom >= 5)
                        }
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .glassEffect(.regular, in: .capsule)
                    }
                }

                // Band / Mode quick controls
                VStack(spacing: 10) {
                    DeltaRow(label: "Band", value: radio.bandName) { ble.sendDelta("band", delta: $0) }
                    DeltaRow(label: "Mode", value: radio.modeName) { ble.sendDelta("mode", delta: $0) }
                }

                Divider().opacity(0.4)

                // Frequency range — proper labelled fields
                VStack(alignment: .leading, spacing: 10) {
                    Text("Range".uppercased())
                        .font(.caption2.weight(.semibold))
                        .tracking(1.2)
                        .foregroundStyle(.secondary)

                    HStack(spacing: 8) {
                        rangeField(placeholder: radio.isFM ? "87.5" : "530", text: $startFreqText, label: "Start")
                        rangeField(placeholder: radio.isFM ? "108.0" : "1700", text: $endFreqText, label: "End")
                        Text(radio.isFM ? "MHz" : "kHz")
                            .font(.caption.monospaced())
                            .foregroundStyle(.secondary)
                    }

                    HStack(spacing: 10) {
                        Button("Apply") { applyRange() }
                            .frame(maxWidth: .infinity)
                            .buttonStyle(.glassProminent)
                            .tint(.accent)

                        Button("Reset") {
                            radio.waterfallRows.removeAll()
                            radio.waterfallMeta = nil
                            startFreqText = ""
                            endFreqText = ""
                        }
                        .frame(maxWidth: .infinity)
                        .buttonStyle(.glass)
                    }
                }
            }
        }
    }

    private func rangeField(placeholder: String, text: Binding<String>, label: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
            TextField(placeholder, text: text)
                .font(.callout.monospaced())
                .textFieldStyle(.plain)
                .keyboardType(.decimalPad)
                .padding(.horizontal, 10)
                .padding(.vertical, 8)
                .glassEffect(.regular, in: .rect(cornerRadius: 10))
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
        let hue = (1 - t) * 240.0 / 360.0
        return Color(hue: hue, saturation: 1.0, brightness: 0.10 + 0.90 * t)
    }
}
