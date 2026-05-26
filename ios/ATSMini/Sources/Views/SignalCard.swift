import SwiftUI

struct SignalCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        GlassCard {
            VStack(spacing: 14) {

                CardHeader(title: "Signal & Status", trailing: "Seq \(radio.sequenceNumber)")

                // Signal meters
                VStack(spacing: 10) {
                    MeterRow(label: "RSSI",  value: "\(radio.rssi) dBuV",
                             fraction: Double(radio.rssi) / 127.0,
                             gradient: [.cyan, .green])

                    MeterRow(label: "SNR",   value: "\(radio.snr) dB",
                             fraction: Double(radio.snr) / 30.0,
                             gradient: [.orange, .yellow])
                }

                Divider().opacity(0.4)

                // Battery row
                BatteryRow()

                Divider().opacity(0.4)

                // CPU compact row
                HStack(spacing: 10) {
                    MiniMeter(label: "CPU 0", percent: radio.cpu0,
                              gradient: [.cyan, Color(red: 0, green: 1, blue: 0.8)])
                    MiniMeter(label: "CPU 1", percent: radio.cpu1,
                              gradient: [Color(red: 1, green: 0.4, blue: 0), .orange])
                }
            }
        }
    }
}

// MARK: - Battery Row

private struct BatteryRow: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        VStack(spacing: 6) {
            HStack {
                Image(systemName: batteryIcon)
                    .foregroundStyle(batteryColor)
                    .font(.callout)
                Text("Battery")
                    .font(.subheadline.weight(.medium))
                Spacer()
                Text(String(format: "%.2f V · %d%%", radio.batteryVoltage, Int(radio.batteryPercent)))
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
            }

            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(.tertiary).opacity(0.3)
                    Capsule()
                        .fill(batteryColor)
                        .frame(width: geo.size.width * radio.batteryPercent / 100)
                        .animation(.smooth(duration: 0.3), value: radio.batteryPercent)
                }
            }
            .frame(height: 8)
        }
    }

    private var batteryColor: Color {
        let pct = radio.batteryPercent
        if pct < 20 { return .red }
        if pct < 50 { return .orange }
        return .green
    }

    private var batteryIcon: String {
        let pct = radio.batteryPercent
        if pct < 10 { return "battery.0percent" }
        if pct < 40 { return "battery.25percent" }
        if pct < 65 { return "battery.50percent" }
        if pct < 90 { return "battery.75percent" }
        return "battery.100percent"
    }
}

// MARK: - Mini CPU Meter

private struct MiniMeter: View {
    let label: String
    let percent: Int
    let gradient: [Color]

    var body: some View {
        VStack(spacing: 6) {
            HStack {
                Text(label)
                    .font(.caption2.weight(.medium))
                    .foregroundStyle(.secondary)
                Spacer()
                Text("\(percent)%")
                    .font(.caption2.monospaced())
                    .foregroundStyle(.primary)
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(.tertiary).opacity(0.3)
                    Capsule()
                        .fill(LinearGradient(colors: gradient, startPoint: .leading, endPoint: .trailing))
                        .frame(width: geo.size.width * Double(min(100, max(0, percent))) / 100)
                        .animation(.smooth(duration: 0.3), value: percent)
                }
            }
            .frame(height: 6)
        }
        .frame(maxWidth: .infinity)
        .padding(10)
        .glassEffect(.regular, in: .rect(cornerRadius: 12))
    }
}

// MARK: - Meter Row

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

// MARK: - Stat Box (kept for any remaining uses)

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
