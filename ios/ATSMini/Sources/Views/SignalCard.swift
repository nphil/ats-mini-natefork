import SwiftUI

struct SignalCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        GlassCard {
            VStack(spacing: 10) {
                MeterRow(label: "RSSI", value: "\(radio.rssi) dBuV",
                         fraction: Double(radio.rssi) / 127.0,
                         gradient: [.cyan, .green])

                MeterRow(label: "SNR", value: "\(radio.snr) dB",
                         fraction: Double(radio.snr) / 30.0,
                         gradient: [.orange, .yellow])

                MeterRow(label: "CPU 0", value: "\(radio.cpu0)%",
                         fraction: Double(radio.cpu0) / 100.0,
                         gradient: [.cyan, Color(red: 0, green: 1, blue: 0.8)])

                MeterRow(label: "CPU 1", value: "\(radio.cpu1)%",
                         fraction: Double(radio.cpu1) / 100.0,
                         gradient: [Color(red: 1, green: 0.4, blue: 0), .orange])

                Divider()

                // Stats grid
                HStack(spacing: 12) {
                    StatBox(value: "\(radio.rssi)", label: "RSSI")
                    StatBox(value: "\(radio.snr)", label: "SNR")
                    StatBox(value: String(format: "%.2fV", radio.batteryVoltage), label: "Battery")
                    StatBox(value: "\(radio.sequenceNumber)", label: "Seq")
                }

                // Battery bar
                VStack(alignment: .leading, spacing: 4) {
                    HStack {
                        Text("Battery")
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                        Spacer()
                        Text("\(Int(radio.batteryPercent))%")
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                    GeometryReader { geo in
                        ZStack(alignment: .leading) {
                            RoundedRectangle(cornerRadius: 4)
                                .fill(.ultraThinMaterial)
                            RoundedRectangle(cornerRadius: 4)
                                .fill(batteryColor)
                                .frame(width: geo.size.width * radio.batteryPercent / 100)
                        }
                    }
                    .frame(height: 10)
                }
            }
        }
    }

    var batteryColor: Color {
        let pct = radio.batteryPercent
        if pct < 20 { return .red }
        if pct < 50 { return .orange }
        return .green
    }
}

struct MeterRow: View {
    let label: String
    let value: String
    let fraction: Double
    let gradient: [Color]

    var body: some View {
        VStack(spacing: 3) {
            HStack {
                Text(label)
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                Spacer()
                Text(value)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(.secondary)
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 4)
                        .fill(.ultraThinMaterial)
                    RoundedRectangle(cornerRadius: 4)
                        .fill(LinearGradient(colors: gradient, startPoint: .leading, endPoint: .trailing))
                        .frame(width: geo.size.width * min(1, max(0, fraction)))
                        .animation(.easeOut(duration: 0.3), value: fraction)
                }
            }
            .frame(height: 8)
        }
    }
}

struct StatBox: View {
    let value: String
    let label: String

    var body: some View {
        VStack(spacing: 2) {
            Text(value)
                .font(.system(.caption, design: .monospaced).bold())
                .foregroundStyle(Color.accent)
            Text(label)
                .font(.system(size: 9))
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 6)
        .background(.ultraThinMaterial)
        .clipShape(RoundedRectangle(cornerRadius: 6))
    }
}
