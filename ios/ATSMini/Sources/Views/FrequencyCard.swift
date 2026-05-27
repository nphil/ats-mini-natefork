import SwiftUI

struct FrequencyCard: View {
    @EnvironmentObject var radio: RadioState
    @EnvironmentObject var theme: ThemeStore
    @ObservedObject private var ble = BLEManager.shared
    @State private var quickTuneText = ""
    @State private var showQuickTune = false
    @FocusState private var quickTuneFocused: Bool

    private let impact = UIImpactFeedbackGenerator(style: .medium)

    var body: some View {
        GlassCard(padding: 14) {
            VStack(spacing: 10) {

                // Frequency display
                VStack(spacing: 2) {
                    Text(radio.formattedFrequency)
                        .font(.system(size: 52, weight: .semibold, design: .rounded))
                        .monospacedDigit()
                        .minimumScaleFactor(0.5)
                        .lineLimit(1)
                        .contentTransition(.numericText())
                        .onTapGesture {
                            impact.impactOccurred()
                            showQuickTune.toggle()
                            if showQuickTune { quickTuneFocused = true }
                        }

                    HStack(spacing: 14) {
                        ForEach(FreqUnit.allCases, id: \.self) { unit in
                            Text(unit.rawValue)
                                .font(.caption2.monospaced())
                                .foregroundStyle(radio.freqDisplayUnit == unit
                                                 ? theme.current.accentColor
                                                 : .secondary)
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
                                .font(.body)
                                .frame(width: 44, height: 38)
                        }
                        .buttonStyle(.glass)
                        .tint(theme.current.accentColor)
                        .disabled(!radio.isConnected || radio.isSeeking)

                        Button {
                            UIImpactFeedbackGenerator(style: .light).impactOccurred()
                            ble.sendRaw("r")
                        } label: {
                            Image(systemName: "chevron.left")
                                .font(.body)
                                .frame(maxWidth: .infinity, minHeight: 38)
                        }
                        .buttonStyle(.glass)
                        .disabled(!radio.isConnected)

                        Button {
                            UIImpactFeedbackGenerator(style: .light).impactOccurred()
                            ble.sendRaw("R")
                        } label: {
                            Image(systemName: "chevron.right")
                                .font(.body)
                                .frame(maxWidth: .infinity, minHeight: 38)
                        }
                        .buttonStyle(.glass)
                        .disabled(!radio.isConnected)

                        Button {
                            impact.impactOccurred()
                            ble.sendSeek(1)
                        } label: {
                            Image(systemName: "forward.end.fill")
                                .font(.body)
                                .frame(width: 44, height: 38)
                        }
                        .buttonStyle(.glass)
                        .tint(theme.current.accentColor)
                        .disabled(!radio.isConnected || radio.isSeeking)
                    }
                }

                if radio.isSeeking {
                    HStack(spacing: 6) {
                        ProgressView().controlSize(.mini)
                        Text("Seeking…")
                            .font(.caption2.monospaced())
                            .foregroundStyle(theme.current.accentColor)
                    }
                }

                // Quick tune
                if showQuickTune {
                    HStack(spacing: 8) {
                        TextField("e.g. 98.5 MHz or 7125 kHz", text: $quickTuneText)
                            .font(.system(.body, design: .monospaced))
                            .textFieldStyle(.plain)
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .glassEffect(.regular, in: .rect(cornerRadius: 12))
                            .keyboardType(.decimalPad)
                            .focused($quickTuneFocused)
                            .onSubmit { commitQuickTune() }

                        Button("Tune") {
                            impact.impactOccurred()
                            commitQuickTune()
                        }
                        .buttonStyle(.glassProminent)
                        .tint(theme.current.accentColor)
                        .disabled(quickTuneText.isEmpty)
                    }
                    .transition(.opacity.combined(with: .move(edge: .top)))
                }

                // RDS inline (FM only, when data available)
                if radio.isFM && hasRDS {
                    Divider().opacity(0.35)
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

// MARK: - RDS Inline

private struct RDSInline: View {
    @EnvironmentObject var radio: RadioState
    @EnvironmentObject var theme: ThemeStore

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                if !radio.rdsStation.isEmpty {
                    Text(radio.rdsStation)
                        .font(.subheadline.monospaced().weight(.semibold))
                        .foregroundStyle(theme.current.accentColor)
                }
                if !radio.rdsPTY.isEmpty {
                    Text(radio.rdsPTY)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .padding(.horizontal, 7)
                        .padding(.vertical, 2)
                        .glassEffect(.regular, in: .capsule)
                }
                Spacer()
                if !radio.rdsTime.isEmpty {
                    Text(radio.rdsTime)
                        .font(.caption2.monospaced())
                        .foregroundStyle(.tertiary)
                }
            }

            if !radio.rdsText.isEmpty {
                Text(radio.rdsText)
                    .font(.caption)
                    .foregroundStyle(.primary.opacity(0.85))
                    .lineLimit(2)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
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
