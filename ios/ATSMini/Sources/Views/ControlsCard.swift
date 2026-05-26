import SwiftUI

struct ControlsCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared

    var body: some View {
        GlassCard {
            VStack(spacing: 14) {

                CardHeader(title: "Controls")

                VStack(spacing: 10) {
                    DeltaRow(label: "Band",  value: radio.bandName)  { ble.sendDelta("band", delta: $0) }
                    DeltaRow(label: "Mode",  value: radio.modeName)  { ble.sendDelta("mode", delta: $0) }
                    DeltaRow(label: "Step",  value: radio.stepSize)  { ble.sendDelta("step", delta: $0) }
                    DeltaRow(label: "BW",    value: radio.bandwidth) { ble.sendDelta("bw",   delta: $0) }
                    DeltaRow(label: "AGC",   value: radio.agc)       { ble.sendDelta("agc",  delta: $0) }
                }

                Divider().opacity(0.4)

                VolumeRow()

                Divider().opacity(0.4)

                GlassEffectContainer {
                    HStack(spacing: 10) {
                        Button {
                            UIImpactFeedbackGenerator(style: .medium).impactOccurred()
                            ble.sendClick()
                        } label: {
                            Label("Click", systemImage: "button.programmable")
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 4)
                        }
                        .buttonStyle(.glass)
                        .tint(.accent)
                        .disabled(!radio.isConnected)

                        Button {
                            UIImpactFeedbackGenerator(style: .heavy).impactOccurred()
                            ble.sendSleep(true)
                        } label: {
                            Label("Sleep", systemImage: "moon.fill")
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 4)
                        }
                        .buttonStyle(.glassProminent)
                        .tint(.indigo)
                        .disabled(!radio.isConnected)
                    }
                }
            }
        }
    }
}

// MARK: - Delta Row

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

// MARK: - Volume Row

struct VolumeRow: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var localVolume: Double = 35
    @State private var isDragging = false

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "speaker.fill")
                .font(.callout)
                .foregroundStyle(.secondary)
                .frame(width: 24)

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
            .tint(.accent)

            Image(systemName: "speaker.wave.3.fill")
                .font(.callout)
                .foregroundStyle(.secondary)
                .frame(width: 24)

            Text("\(Int(localVolume))")
                .font(.callout.monospaced().weight(.medium))
                .frame(width: 32, alignment: .trailing)
        }
        .onChange(of: radio.volume) { _, newVal in
            if !isDragging { localVolume = Double(newVal) }
        }
        .onAppear { localVolume = Double(radio.volume) }
    }
}
