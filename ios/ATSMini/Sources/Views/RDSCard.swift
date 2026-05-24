import SwiftUI

struct RDSCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 8) {

                CardHeader(title: "RDS", trailing: radio.rdsTime.isEmpty ? nil : radio.rdsTime)

                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    if !radio.rdsStation.isEmpty {
                        Text(radio.rdsStation)
                            .font(.title3.monospaced().weight(.semibold))
                            .foregroundStyle(.accent)
                    }
                    if !radio.rdsPTY.isEmpty {
                        Text(radio.rdsPTY)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .padding(.horizontal, 8)
                            .padding(.vertical, 3)
                            .glassEffect(.regular, in: .capsule)
                    }
                }

                if !radio.rdsText.isEmpty {
                    Text(radio.rdsText)
                        .font(.subheadline)
                        .foregroundStyle(.primary)
                        .lineLimit(3)
                }
            }
        }
    }
}
