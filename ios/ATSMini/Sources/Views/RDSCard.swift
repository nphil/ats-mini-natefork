import SwiftUI

struct RDSCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    if !radio.rdsStation.isEmpty {
                        Text(radio.rdsStation)
                            .font(.system(.headline, design: .monospaced))
                            .foregroundStyle(Color.accent)
                    }
                    Spacer()
                    if !radio.rdsTime.isEmpty {
                        Text(radio.rdsTime)
                            .font(.system(.caption, design: .monospaced))
                            .foregroundStyle(Color(red: 0, green: 1, blue: 0.25))
                    }
                }
                if !radio.rdsPTY.isEmpty {
                    Text(radio.rdsPTY)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                if !radio.rdsText.isEmpty {
                    Text(radio.rdsText)
                        .font(.caption2)
                        .foregroundStyle(.primary)
                        .lineLimit(2)
                }
            }
        }
    }
}
