import SwiftUI

/// Compact one-card signal/battery/CPU status. Designed to fit in ~110pt
/// vertical without scrolling.
struct SignalCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        GlassCard(padding: 12) {
            VStack(spacing: 10) {
                // Top: RSSI / SNR / Battery as 3 mini meters in a row
                HStack(spacing: 10) {
                    MiniMeter(icon: "antenna.radiowaves.left.and.right",
                              label: "RSSI",
                              value: "\(radio.rssi)",
                              fraction: Double(radio.rssi) / 127.0,
                              gradient: [.cyan, .green])

                    MiniMeter(icon: "waveform.badge.magnifyingglass",
                              label: "SNR",
                              value: "\(radio.snr) dB",
                              fraction: Double(radio.snr) / 30.0,
                              gradient: [.orange, .yellow])

                    MiniMeter(icon: batteryIcon,
                              label: "Batt",
                              value: "\(Int(radio.batteryPercent))%",
                              fraction: radio.batteryPercent / 100.0,
                              gradient: batteryGradient,
                              valueTint: batteryColor)
                }

                // Bottom row: CPU 0 + CPU 1 + Seq (compact)
                HStack(spacing: 10) {
                    CpuPill(label: "CPU 0", percent: radio.cpu0,
                            gradient: [.cyan, Color(red: 0, green: 1, blue: 0.8)])
                    CpuPill(label: "CPU 1", percent: radio.cpu1,
                            gradient: [Color(red: 1, green: 0.4, blue: 0), .orange])
                    Spacer(minLength: 0)
                    HStack(spacing: 4) {
                        Image(systemName: "number")
                            .font(.caption2)
                            .foregroundStyle(.tertiary)
                        Text("\(radio.sequenceNumber)")
                            .font(.caption2.monospaced())
                            .foregroundStyle(.secondary)
                    }
                }
            }
        }
    }

    private var batteryColor: Color {
        let pct = radio.batteryPercent
        if pct < 20 { return .red }
        if pct < 50 { return .orange }
        return .green
    }

    private var batteryGradient: [Color] {
        let pct = radio.batteryPercent
        if pct < 20 { return [.red, .red] }
        if pct < 50 { return [.orange, .yellow] }
        return [.green, .green]
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

// MARK: - Mini Meter (RSSI / SNR / Battery)

private struct MiniMeter: View {
    let icon: String
    let label: String
    let value: String
    let fraction: Double
    let gradient: [Color]
    var valueTint: Color? = nil

    var body: some View {
        VStack(spacing: 5) {
            HStack(spacing: 4) {
                Image(systemName: icon)
                    .font(.caption2)
                    .foregroundStyle(valueTint ?? .secondary)
                Text(label)
                    .font(.caption2.weight(.medium))
                    .foregroundStyle(.secondary)
                Spacer(minLength: 0)
                Text(value)
                    .font(.caption2.monospaced().weight(.medium))
                    .foregroundStyle(valueTint ?? .primary)
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
            .frame(height: 5)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 6)
        .padding(.horizontal, 8)
        .glassEffect(.regular, in: .rect(cornerRadius: 10))
    }
}

// MARK: - CPU Pill (small inline percent)

private struct CpuPill: View {
    let label: String
    let percent: Int
    let gradient: [Color]

    var body: some View {
        HStack(spacing: 6) {
            Text(label)
                .font(.caption2.weight(.medium))
                .foregroundStyle(.secondary)
            ZStack(alignment: .leading) {
                Capsule().fill(.tertiary).opacity(0.3)
                Capsule()
                    .fill(LinearGradient(colors: gradient,
                                         startPoint: .leading, endPoint: .trailing))
                    .frame(width: 36 * Double(min(100, max(0, percent))) / 100)
                    .animation(.smooth(duration: 0.3), value: percent)
            }
            .frame(width: 36, height: 4)
            Text("\(percent)%")
                .font(.caption2.monospaced())
                .foregroundStyle(.primary)
        }
    }
}

// MARK: - Legacy MeterRow / StatBox (kept in case other files import them)

struct MeterRow: View {
    let label: String
    let value: String
    let fraction: Double
    let gradient: [Color]

    var body: some View {
        VStack(spacing: 4) {
            HStack {
                Text(label).font(.caption.weight(.medium)).foregroundStyle(.secondary)
                Spacer()
                Text(value).font(.caption.monospaced()).foregroundStyle(.primary)
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(.tertiary).opacity(0.3)
                    Capsule()
                        .fill(LinearGradient(colors: gradient, startPoint: .leading, endPoint: .trailing))
                        .frame(width: geo.size.width * min(1, max(0, fraction)))
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
            Text(value).font(.title3.monospaced().weight(.semibold)).foregroundStyle(.accent)
            Text(label).font(.caption2.weight(.medium)).foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 10)
        .glassEffect(.regular, in: .rect(cornerRadius: 12))
    }
}
