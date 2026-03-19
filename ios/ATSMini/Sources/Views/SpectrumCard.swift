import SwiftUI

struct SpectrumCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var selectedStep = 10

    private let stepOptions = [
        (5, "Narrow (1 MHz)"),
        (10, "Normal (2 MHz)"),
        (25, "Wide (5 MHz)"),
        (50, "Full (10 MHz)")
    ]

    var body: some View {
        GlassCard {
            VStack(spacing: 10) {
                // Header
                HStack {
                    Text("SPECTRUM")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .tracking(1)
                    Spacer()
                    if radio.isScanning {
                        Text("Scanning... \(Int(radio.scanProgress))%")
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    } else if let scan = radio.scanData {
                        Text("\(scan.pointCount) pts")
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                }

                // Scan progress
                if radio.isScanning {
                    ProgressView(value: radio.scanProgress, total: 100)
                        .tint(.accent)
                }

                // Controls
                HStack(spacing: 8) {
                    Picker("Step", selection: $selectedStep) {
                        ForEach(stepOptions, id: \.0) { option in
                            Text(option.1).tag(option.0)
                        }
                    }
                    .pickerStyle(.menu)
                    .font(.caption)

                    Button {
                        ble.sendScan(step: selectedStep)
                    } label: {
                        Label("Scan", systemImage: "play.fill")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                    .tint(.accent)
                    .disabled(!radio.isConnected || radio.isScanning)

                    Button {
                        radio.scanData = nil
                    } label: {
                        Image(systemName: "xmark")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                    .tint(.red)
                }

                // Canvas
                SpectrumCanvas()
                    .frame(height: 160)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
            }
        }
    }
}

struct SpectrumCanvas: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared

    var body: some View {
        Canvas { context, size in
            let W = size.width
            let H = size.height

            // Background
            context.fill(
                Path(CGRect(origin: .zero, size: size)),
                with: .color(Color(red: 0.04, green: 0.1, blue: 0.04))
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
                    context.fill(Path(rect), with: .color(Color(red: 0, green: 0.3, blue: 0.47).opacity(0.6)))
                }
            }

            // RSSI bars
            for (i, v) in scan.rssi.enumerated() {
                let h = (Double(v) / maxR) * (H - 20)
                let t = h / (H - 20)
                let color = Color(red: 0, green: 0.1 + 0.9 * t, blue: 0.1 * t)
                let rect = CGRect(x: Double(i) * barW + 0.5, y: H - h, width: max(1, barW - 1), height: h)
                context.fill(Path(rect), with: .color(color))
            }

            // Channel markers
            for freq in scan.channels {
                let i = Double(freq - scan.startFreq) / Double(scan.step)
                guard i >= 0, i < Double(n) else { continue }
                let x = (i + 0.5) * barW

                // Dashed line
                var linePath = Path()
                linePath.move(to: CGPoint(x: x, y: 0))
                linePath.addLine(to: CGPoint(x: x, y: H - 20))
                context.stroke(linePath, with: .color(.cyan),
                              style: StrokeStyle(lineWidth: 1, dash: [3, 3]))

                // Label
                let label = String(format: "%.1f", Double(freq) / 100.0)
                context.draw(Text(label).font(.system(size: 8, design: .monospaced)).foregroundColor(.cyan),
                            at: CGPoint(x: x, y: 8))
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
                Text("\(scan.startFreq) kHz").font(.system(size: 8, design: .monospaced)).foregroundColor(Color(red: 0.2, green: 0.4, blue: 0.2)),
                at: CGPoint(x: 30, y: H - 6)
            )
            let endFreq = scan.startFreq + scan.step * (n - 1)
            context.draw(
                Text("\(endFreq) kHz").font(.system(size: 8, design: .monospaced)).foregroundColor(Color(red: 0.2, green: 0.4, blue: 0.2)),
                at: CGPoint(x: W - 30, y: H - 6)
            )
        }
        .onTapGesture { _ in
            // Tap-to-tune requires GeometryReader; implemented below via overlay
        }
    }
}
