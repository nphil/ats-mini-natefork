import SwiftUI

struct SpectrumCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var selectedStep = 10

    private let stepOptions = [
        (5,  "Narrow",   "1 MHz"),
        (10, "Normal",   "2 MHz"),
        (25, "Wide",     "5 MHz"),
        (50, "Full",     "10 MHz")
    ]

    private var selectedStepLabel: String {
        if let opt = stepOptions.first(where: { $0.0 == selectedStep }) {
            return "\(opt.1) · \(opt.2)"
        }
        return "Normal"
    }

    var body: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 14) {

                // Header
                CardHeader(
                    title: "Spectrum",
                    trailing: radio.isScanning
                        ? "Scanning \(Int(radio.scanProgress))%"
                        : (radio.scanData.map { "\($0.pointCount) pts" } ?? nil)
                )

                if radio.isScanning {
                    ProgressView(value: radio.scanProgress, total: 100)
                        .tint(.accent)
                }

                // Step selector (own row, room to breathe)
                LabeledContent("Step") {
                    Menu {
                        ForEach(stepOptions, id: \.0) { opt in
                            Button {
                                selectedStep = opt.0
                            } label: {
                                if opt.0 == selectedStep {
                                    Label("\(opt.1) (\(opt.2))", systemImage: "checkmark")
                                } else {
                                    Text("\(opt.1) (\(opt.2))")
                                }
                            }
                        }
                    } label: {
                        HStack(spacing: 6) {
                            Text(selectedStepLabel)
                                .font(.subheadline.weight(.medium))
                            Image(systemName: "chevron.up.chevron.down")
                                .font(.caption2)
                        }
                    }
                    .menuStyle(.button)
                    .buttonStyle(.glass)
                    .disabled(radio.isScanning)
                }

                // Canvas — give it height
                SpectrumCanvas()
                    .frame(height: 200)
                    .clipShape(.rect(cornerRadius: 14))

                // Action row — full-width primary, compact destructive
                GlassEffectContainer {
                    HStack(spacing: 10) {
                        Button {
                            ble.sendScan(step: selectedStep)
                        } label: {
                            Label(radio.isScanning ? "Scanning…" : "Start Scan", systemImage: "play.fill")
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 4)
                        }
                        .buttonStyle(.glassProminent)
                        .tint(.accent)
                        .disabled(!radio.isConnected || radio.isScanning)

                        Button(role: .destructive) {
                            radio.scanData = nil
                        } label: {
                            Image(systemName: "trash")
                                .frame(width: 44, height: 44)
                        }
                        .buttonStyle(.glass)
                        .disabled(radio.scanData == nil)
                    }
                }
            }
        }
    }
}

struct SpectrumCanvas: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        Canvas { context, size in
            let W = size.width
            let H = size.height

            // Background
            context.fill(
                Path(CGRect(origin: .zero, size: size)),
                with: .color(Color(red: 0.03, green: 0.06, blue: 0.10))
            )

            guard let scan = radio.scanData, !scan.rssi.isEmpty else { return }

            let n = scan.pointCount
            let maxR = Double(scan.rssi.max() ?? 1)
            let barW = W / Double(n)

            // SNR bars (dim)
            if !scan.snr.isEmpty {
                for (i, v) in scan.snr.enumerated() {
                    let h = (Double(v) / maxR) * (H - 20)
                    let rect = CGRect(x: Double(i) * barW, y: H - h, width: barW, height: h)
                    context.fill(Path(rect), with: .color(Color(red: 0, green: 0.3, blue: 0.47).opacity(0.55)))
                }
            }

            // RSSI bars
            for (i, v) in scan.rssi.enumerated() {
                let h = (Double(v) / maxR) * (H - 20)
                let t = h / (H - 20)
                let color = Color(hue: 0.5 - 0.35 * t, saturation: 0.85, brightness: 0.4 + 0.55 * t)
                let rect = CGRect(x: Double(i) * barW + 0.5, y: H - h, width: max(1, barW - 1), height: h)
                context.fill(Path(rect), with: .color(color))
            }

            // Channel markers
            for freq in scan.channels {
                let i = Double(freq - scan.startFreq) / Double(scan.step)
                guard i >= 0, i < Double(n) else { continue }
                let x = (i + 0.5) * barW

                var linePath = Path()
                linePath.move(to: CGPoint(x: x, y: 0))
                linePath.addLine(to: CGPoint(x: x, y: H - 20))
                context.stroke(
                    linePath,
                    with: .color(.accent.opacity(0.8)),
                    style: StrokeStyle(lineWidth: 1, dash: [3, 3])
                )

                let label = String(format: "%.1f", Double(freq) / 100.0)
                context.draw(
                    Text(label).font(.system(size: 9, design: .monospaced)).foregroundColor(.accent),
                    at: CGPoint(x: x, y: 10)
                )
            }

            // Current freq marker
            if radio.frequency > 0 {
                let i = Double(radio.frequency - scan.startFreq) / Double(scan.step)
                if i >= 0, i < Double(n) {
                    let x = (i + 0.5) * barW
                    var freqPath = Path()
                    freqPath.move(to: CGPoint(x: x, y: 0))
                    freqPath.addLine(to: CGPoint(x: x, y: H))
                    context.stroke(freqPath, with: .color(.red), lineWidth: 2)
                }
            }

            // Axis labels
            context.draw(
                Text("\(scan.startFreq) kHz").font(.system(size: 9, design: .monospaced)).foregroundColor(.secondary),
                at: CGPoint(x: 38, y: H - 8)
            )
            let endFreq = scan.startFreq + scan.step * (n - 1)
            context.draw(
                Text("\(endFreq) kHz").font(.system(size: 9, design: .monospaced)).foregroundColor(.secondary),
                at: CGPoint(x: W - 38, y: H - 8)
            )
        }
    }
}
