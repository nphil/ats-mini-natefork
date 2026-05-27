import SwiftUI

struct ControlsCard: View {
    @EnvironmentObject var radio: RadioState
    @EnvironmentObject var theme: ThemeStore
    @ObservedObject private var ble = BLEManager.shared

    var body: some View {
        GlassCard(padding: 14) {
            VStack(spacing: 12) {

                // Five compact menu pills — value visible, tap to step
                FlowLayout(spacing: 8) {
                    ParamMenu(label: "Band",  value: radio.bandName)  { ble.sendDelta("band", delta: $0) }
                    ParamMenu(label: "Mode",  value: radio.modeName)  { ble.sendDelta("mode", delta: $0) }
                    ParamMenu(label: "Step",  value: radio.stepSize)  { ble.sendDelta("step", delta: $0) }
                    ParamMenu(label: "BW",    value: radio.bandwidth) { ble.sendDelta("bw",   delta: $0) }
                    ParamMenu(label: "AGC",   value: radio.agc)       { ble.sendDelta("agc",  delta: $0) }
                }

                VolumeRow()

                // Compact icon-only action row
                HStack(spacing: 10) {
                    Button {
                        UIImpactFeedbackGenerator(style: .medium).impactOccurred()
                        ble.sendClick()
                    } label: {
                        Label("Click", systemImage: "button.programmable")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 2)
                    }
                    .buttonStyle(.glass)
                    .tint(theme.current.accentColor)
                    .disabled(!radio.isConnected)

                    Button {
                        UIImpactFeedbackGenerator(style: .heavy).impactOccurred()
                        ble.sendSleep(true)
                    } label: {
                        Label("Sleep", systemImage: "moon.fill")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 2)
                    }
                    .buttonStyle(.glassProminent)
                    .tint(.indigo)
                    .disabled(!radio.isConnected)
                }
            }
        }
    }
}

// MARK: - Param Menu Pill

private struct ParamMenu: View {
    let label: String
    let value: String
    let action: (Int) -> Void
    @EnvironmentObject var radio: RadioState

    private let impact = UIImpactFeedbackGenerator(style: .light)

    var body: some View {
        Menu {
            Button {
                impact.impactOccurred()
                action(1)
            } label: {
                Label("Next", systemImage: "chevron.right")
            }
            Button {
                impact.impactOccurred()
                action(-1)
            } label: {
                Label("Previous", systemImage: "chevron.left")
            }
        } label: {
            HStack(spacing: 6) {
                Text(label)
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                    .textCase(.uppercase)
                    .tracking(0.6)
                Text(value)
                    .font(.callout.monospaced().weight(.medium))
                    .foregroundStyle(.primary)
                    .lineLimit(1)
                Image(systemName: "chevron.up.chevron.down")
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }
            .padding(.horizontal, 11)
            .padding(.vertical, 7)
            .glassEffect(.regular, in: .capsule)
        }
        .menuStyle(.button)
        .disabled(!radio.isConnected)
    }
}

// MARK: - Compact Volume Row

struct VolumeRow: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var localVolume: Double = 35
    @State private var isDragging = false

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "speaker.fill")
                .font(.caption)
                .foregroundStyle(.secondary)

            Slider(value: $localVolume, in: 0...100, step: 1) { editing in
                if !editing && isDragging {
                    isDragging = false
                    let delta = Int(localVolume) - radio.volume
                    if delta != 0 {
                        UIImpactFeedbackGenerator(style: .light).impactOccurred()
                        for _ in 0..<abs(delta) {
                            ble.sendVolumeDelta(delta > 0 ? 1 : -1)
                        }
                    }
                } else {
                    isDragging = true
                }
            }

            Image(systemName: "speaker.wave.3.fill")
                .font(.caption)
                .foregroundStyle(.secondary)

            Text("\(Int(localVolume))")
                .font(.caption.monospaced().weight(.medium))
                .frame(width: 28, alignment: .trailing)
        }
        .onChange(of: radio.volume) { _, newVal in
            if !isDragging { localVolume = Double(newVal) }
        }
        .onAppear { localVolume = Double(radio.volume) }
    }
}

// Kept for compatibility with WaterfallCard's "Band/Mode" quick controls.
struct DeltaRow: View {
    let label: String
    let value: String
    let action: (Int) -> Void
    @EnvironmentObject var radio: RadioState

    private let lightImpact = UIImpactFeedbackGenerator(style: .light)

    var body: some View {
        HStack(spacing: 10) {
            Text(label)
                .font(.subheadline.weight(.medium))
                .foregroundStyle(.secondary)
                .frame(width: 58, alignment: .leading)

            GlassEffectContainer {
                HStack(spacing: 4) {
                    Button {
                        lightImpact.impactOccurred()
                        action(-1)
                    } label: {
                        Image(systemName: "minus")
                            .font(.callout.weight(.semibold))
                            .frame(width: 36, height: 32)
                    }
                    .buttonStyle(.glass)
                    .disabled(!radio.isConnected)

                    Text(value)
                        .font(.callout.monospaced().weight(.medium))
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)
                        .frame(maxWidth: .infinity)
                        .frame(height: 32)

                    Button {
                        lightImpact.impactOccurred()
                        action(1)
                    } label: {
                        Image(systemName: "plus")
                            .font(.callout.weight(.semibold))
                            .frame(width: 36, height: 32)
                    }
                    .buttonStyle(.glass)
                    .disabled(!radio.isConnected)
                }
            }
        }
    }
}
