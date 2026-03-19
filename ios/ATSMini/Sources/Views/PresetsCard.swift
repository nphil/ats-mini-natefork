import SwiftUI

struct PresetsCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var presetName = ""
    @State private var editingPresetIdx: Int?
    @State private var editingName = ""

    var body: some View {
        GlassCard {
            VStack(spacing: 10) {
                // Header
                HStack {
                    Text("PRESETS")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .tracking(1)
                    Spacer()
                    Button {
                        ble.sendListPresets()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                            .font(.caption)
                    }
                    .foregroundStyle(Color.accent)
                }

                // Save row
                HStack {
                    TextField("Preset name...", text: $presetName)
                        .font(.system(.caption, design: .monospaced))
                        .textFieldStyle(.roundedBorder)
                    Button("Save") {
                        let name = presetName.isEmpty ? "Preset" : presetName
                        ble.sendSavePreset(name: String(name.prefix(19)))
                        presetName = ""
                    }
                    .font(.caption.bold())
                    .buttonStyle(.bordered)
                    .tint(.green)
                    .disabled(!radio.isConnected || radio.scanData == nil)
                }

                // Preset list
                if !radio.presets.isEmpty {
                    Divider()
                    ForEach(radio.presets) { preset in
                        HStack {
                            if editingPresetIdx == preset.idx {
                                TextField("Name", text: $editingName)
                                    .font(.system(.caption, design: .monospaced))
                                    .textFieldStyle(.roundedBorder)
                                    .onSubmit {
                                        let newName = String(editingName.prefix(19))
                                        if !newName.isEmpty, newName != preset.name {
                                            ble.sendRenamePreset(idx: preset.idx, name: newName)
                                        }
                                        editingPresetIdx = nil
                                    }
                            } else {
                                Text(preset.name)
                                    .font(.system(.caption, design: .monospaced))
                                    .onTapGesture {
                                        editingPresetIdx = preset.idx
                                        editingName = preset.name
                                    }
                            }

                            Spacer()

                            Text("\(preset.channelCount) ch")
                                .font(.caption2)
                                .foregroundStyle(.secondary)

                            Button("Load") {
                                ble.sendLoadPreset(idx: preset.idx)
                            }
                            .font(.caption2)
                            .buttonStyle(.bordered)
                            .tint(.accent)

                            Button {
                                ble.sendDeletePreset(idx: preset.idx)
                            } label: {
                                Image(systemName: "xmark")
                                    .font(.caption2)
                            }
                            .buttonStyle(.bordered)
                            .tint(.red)
                        }
                    }
                }
            }
        }
    }
}
