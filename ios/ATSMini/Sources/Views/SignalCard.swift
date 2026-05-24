import SwiftUI

struct SignalCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        GlassCard {
            VStack(spacing: 14) {

                CardHeader(title: "Signal")

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
                }

                Divider().opacity(0.4)

                // Battery
                VStack(spacing: 6) {
                    HStack {
                        Label("Battery", systemImage: "battery.100")
                            .font(.subheadline.weight(.medium))
                        Spacer()
                        Text(String(format: "%.2f V · %d%%", radio.batteryVoltage, Int(radio.batteryPercent)))
                            .font(.caption.monospaced())
                            .foregroundStyle(.secondary)
                    }

                    GeometryReader { geo in
                        ZStack(alignment: .leading) {
                            Capsule()
                                .fill(.tertiary)
                                .opacity(0.3)
                            Capsule()
                                .fill(batteryColor)
                                .frame(width: geo.size.width * radio.batteryPercent / 100)
                                .animation(.smooth(duration: 0.3), value: radio.batteryPercent)
                        }
                    }
                    .frame(height: 8)
                }

                // Compact stat row
                HStack(spacing: 10) {
                    StatBox(value: "\(radio.rssi)", label: "RSSI")
                    StatBox(value: "\(radio.snr)",  label: "SNR")
                    StatBox(value: "\(radio.sequenceNumber)", label: "Seq")
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
        VStack(spacing: 4) {
            HStack {
                Text(label)
                    .font(.caption.weight(.medium))
                    .foregroundStyle(.secondary)
                Spacer()
                Text(value)
                    .font(.caption.monospaced())
                    .foregroundStyle(.primary)
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(.tertiary).opacity(0.3)
                    Capsule()
                        .fill(LinearGradient(colors: gradient, startPoint: .leading, endPoint: .trailing))
                        .frame(width: geo.size.width * min(1, max(0, fraction)))
                        .animation(.smooth(duration: 0.3), value: fraction)
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
                .font(.title3.monospaced().weight(.semibold))
                .foregroundStyle(.accent)
                .lineLimit(1)
                .minimumScaleFactor(0.6)
            Text(label)
                .font(.caption2.weight(.medium))
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 10)
        .glassEffect(.regular, in: .rect(cornerRadius: 12))
    }
}
