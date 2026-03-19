import SwiftUI

struct ControlsCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared

    var body: some View {
        GlassCard {
            VStack(spacing: 10) {
                DeltaRow(label: "Band", value: radio.bandName) { ble.sendDelta("band", delta: $0) }
                DeltaRow(label: "Mode", value: radio.modeName) { ble.sendDelta("mode", delta: $0) }
                DeltaRow(label: "Step", value: radio.stepSize) { ble.sendDelta("step", delta: $0) }
                DeltaRow(label: "BW", value: radio.bandwidth) { ble.sendDelta("bw", delta: $0) }
                DeltaRow(label: "AGC", value: radio.agc) { ble.sendDelta("agc", delta: $0) }

                Divider()

                // Volume
                VolumeRow()

                Divider()

                // Action buttons
                HStack(spacing: 12) {
                    Button {
                        ble.sendClick()
                    } label: {
                        Label("Encoder Click", systemImage: "button.programmable")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                    .tint(.accent)

                    Button {
                        ble.sendSleep(true)
                    } label: {
                        Label("Sleep", systemImage: "moon.fill")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                    .tint(.red)
                }
            }
        }
    }
}

struct DeltaRow: View {
    let label: String
    let value: String
    let action: (Int) -> Void
    @EnvironmentObject var radio: RadioState

    var body: some View {
        HStack {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
                .frame(width: 50, alignment: .leading)
            Button { action(-1) } label: {
                Image(systemName: "chevron.left")
                    .font(.caption)
                    .frame(width: 32, height: 28)
            }
            .buttonStyle(.bordered)
            .disabled(!radio.isConnected)

            Text(value)
                .font(.system(.caption, design: .monospaced))
                .frame(minWidth: 80)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(.ultraThinMaterial)
                .clipShape(RoundedRectangle(cornerRadius: 4))

            Button { action(1) } label: {
                Image(systemName: "chevron.right")
                    .font(.caption)
                    .frame(width: 32, height: 28)
            }
            .buttonStyle(.bordered)
            .disabled(!radio.isConnected)
        }
    }
}

struct VolumeRow: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var localVolume: Double = 35
    @State private var isDragging = false

    var body: some View {
        HStack {
            Image(systemName: "speaker.fill")
                .font(.caption)
                .foregroundStyle(.secondary)
            Slider(value: $localVolume, in: 0...100, step: 1) { editing in
                if !editing && isDragging {
                    isDragging = false
                    let delta = Int(localVolume) - radio.volume
                    if delta != 0 {
                        for _ in 0..<abs(delta) {
                            ble.sendVolumeDelta(delta > 0 ? 1 : -1)
                        }
                    }
                } else {
                    isDragging = true
                }
            }
            .tint(.accent)
            Text("\(Int(localVolume))")
                .font(.system(.caption, design: .monospaced))
                .frame(width: 30)
        }
        .onChange(of: radio.volume) { _, newVal in
            if !isDragging { localVolume = Double(newVal) }
        }
        .onAppear { localVolume = Double(radio.volume) }
    }
}
