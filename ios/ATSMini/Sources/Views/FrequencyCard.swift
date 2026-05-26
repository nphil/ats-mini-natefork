import SwiftUI

struct FrequencyCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var quickTuneText = ""
    @State private var showQuickTune = false
    @FocusState private var quickTuneFocused: Bool

    private let impact = UIImpactFeedbackGenerator(style: .medium)

    var body: some View {
        GlassCard {
            VStack(spacing: 16) {

                // Frequency display
                VStack(spacing: 6) {
                    Text(radio.formattedFrequency)
                        .font(.system(size: 56, weight: .semibold, design: .rounded))
                        .monospacedDigit()
                        .minimumScaleFactor(0.5)
                        .lineLimit(1)
                        .foregroundStyle(.primary)
                        .contentTransition(.numericText())
                        .onTapGesture {
                            impact.impactOccurred()
                            showQuickTune.toggle()
                            if showQuickTune { quickTuneFocused = true }
                        }

                    HStack(spacing: 14) {
                        ForEach(FreqUnit.allCases, id: \.self) { unit in
                            Text(unit.rawValue)
                                .font(.caption.monospaced())
                                .foregroundStyle(radio.freqDisplayUnit == unit ? Color.accent : .secondary)
                                .onTapGesture { radio.freqDisplayUnit = unit }
                        }
                    }
                }
                .frame(maxWidth: .infinity)

                // Seek controls
                GlassEffectContainer {
                    HStack(spacing: 8) {
                        Button {
                            impact.impactOccurred()
                            ble.sendSeek(-1)
                        } label: {
                            Image(systemName: "backward.end.fill")
                                .font(.title3)
                                .frame(width: 48, height: 44)
                        }
                        .buttonStyle(.glass)
                        .tint(.accent)
                        .disabled(!radio.isConnected || radio.isSeeking)

                        Button {
                            UIImpactFeedbackGenerator(style: .light).impactOccurred()
                            ble.sendRaw("r")
                        } label: {
                            Image(systemName: "chevron.left")
                                .font(.title3)
                                .frame(maxWidth: .infinity, minHeight: 44)
                        }
                        .buttonStyle(.glass)
                        .disabled(!radio.isConnected)

                        Button {
                            UIImpactFeedbackGenerator(style: .light).impactOccurred()
                            ble.sendRaw("R")
                        } label: {
                            Image(systemName: "chevron.right")
                                .font(.title3)
                                .frame(maxWidth: .infinity, minHeight: 44)
                        }
                        .buttonStyle(.glass)
                        .disabled(!radio.isConnected)

                        Button {
                            impact.impactOccurred()
                            ble.sendSeek(1)
                        } label: {
                            Image(systemName: "forward.end.fill")
                                .font(.title3)
                                .frame(width: 48, height: 44)
                        }
                        .buttonStyle(.glass)
                        .tint(.accent)
                        .disabled(!radio.isConnected || radio.isSeeking)
                    }
                }

                if radio.isSeeking {
                    HStack(spacing: 6) {
                        ProgressView().controlSize(.mini)
                        Text("Seeking…")
                            .font(.caption.monospaced())
                            .foregroundStyle(.accent)
                    }
                }

                // Mode info chips
                FlowLayout(spacing: 8) {
                    InfoChip(label: "Band", value: radio.bandName)
                    InfoChip(label: "Mode", value: radio.modeName)
                    InfoChip(label: "Step", value: radio.stepSize)
                    InfoChip(label: "BW",   value: radio.bandwidth)
                }

                // Quick tune
                if showQuickTune {
                    HStack(spacing: 8) {
                        TextField("e.g. 98.5 MHz or 7125 kHz", text: $quickTuneText)
                            .font(.system(.body, design: .monospaced))
                            .textFieldStyle(.plain)
                            .padding(.horizontal, 12)
                            .padding(.vertical, 10)
                            .glassEffect(.regular, in: .rect(cornerRadius: 12))
                            .keyboardType(.decimalPad)
                            .focused($quickTuneFocused)
                            .onSubmit { commitQuickTune() }

                        Button("Tune") {
                            impact.impactOccurred()
                            commitQuickTune()
                        }
                        .buttonStyle(.glassProminent)
                        .tint(.accent)
                        .disabled(quickTuneText.isEmpty)
                    }
                    .transition(.opacity.combined(with: .move(edge: .top)))
                }

                // RDS inline (FM only, when data available)
                if radio.isFM && hasRDS {
                    Divider().opacity(0.4)
                    RDSInline()
                }
            }
            .animation(.smooth(duration: 0.25), value: showQuickTune)
            .animation(.smooth(duration: 0.3), value: hasRDS)
        }
    }

    private var hasRDS: Bool {
        !radio.rdsStation.isEmpty || !radio.rdsPTY.isEmpty || !radio.rdsText.isEmpty
    }

    private func commitQuickTune() {
        guard let freq = parseQuickTune(quickTuneText) else { return }
        ble.sendFrequency(freq)
        quickTuneText = ""
        showQuickTune = false
        quickTuneFocused = false
    }

    private func parseQuickTune(_ s: String) -> Int? {
        let cleaned = s.trimmingCharacters(in: .whitespaces).replacingOccurrences(of: ",", with: "")
        guard !cleaned.isEmpty else { return nil }
        let lower = cleaned.lowercased()
        let hasMHz = lower.contains("mhz") || lower.hasSuffix("m")
        let hasKHz = lower.contains("khz") || lower.hasSuffix("k")
        guard let v = Double(cleaned.filter { $0.isNumber || $0 == "." }), v > 0 else { return nil }
        let inMHz = hasMHz || (!hasKHz && v < 200)
        if radio.isFM {
            return inMHz ? Int(round(v * 100)) : Int(round(v / 10))
        } else {
            return inMHz ? Int(round(v * 1000)) : Int(round(v))
        }
    }
}

// MARK: - RDS Inline (shown inside FrequencyCard)

private struct RDSInline: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("RDS")
                    .font(.caption2.weight(.semibold))
                    .tracking(1.2)
                    .foregroundStyle(.secondary)
                    .textCase(.uppercase)
                Spacer()
                if !radio.rdsTime.isEmpty {
                    Text(radio.rdsTime)
                        .font(.caption2.monospaced())
                        .foregroundStyle(.tertiary)
                }
            }

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
                    .foregroundStyle(.primary.opacity(0.85))
                    .lineLimit(2)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

// MARK: - Info Chip

struct InfoChip: View {
    let label: String
    let value: String

    var body: some View {
        HStack(spacing: 6) {
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
            Text(value)
                .font(.caption.monospaced().weight(.medium))
                .foregroundStyle(.primary)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 5)
        .glassEffect(.regular, in: .capsule)
    }
}

// MARK: - Flow Layout

struct FlowLayout: Layout {
    var spacing: CGFloat = 8

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let maxWidth = proposal.width ?? .infinity
        var lineWidth: CGFloat = 0
        var lineHeight: CGFloat = 0
        var totalHeight: CGFloat = 0
        var widest: CGFloat = 0

        for v in subviews {
            let s = v.sizeThatFits(.unspecified)
            if lineWidth + s.width > maxWidth, lineWidth > 0 {
                totalHeight += lineHeight + spacing
                widest = max(widest, lineWidth - spacing)
                lineWidth = 0
                lineHeight = 0
            }
            lineWidth += s.width + spacing
            lineHeight = max(lineHeight, s.height)
        }
        totalHeight += lineHeight
        widest = max(widest, lineWidth - spacing)
        return CGSize(width: widest, height: totalHeight)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var x = bounds.minX
        var y = bounds.minY
        var lineHeight: CGFloat = 0

        for v in subviews {
            let s = v.sizeThatFits(.unspecified)
            if x + s.width > bounds.maxX, x > bounds.minX {
                x = bounds.minX
                y += lineHeight + spacing
                lineHeight = 0
            }
            v.place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(s))
            x += s.width + spacing
            lineHeight = max(lineHeight, s.height)
        }
    }
}
