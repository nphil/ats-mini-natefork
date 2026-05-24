import SwiftUI

struct PresetsCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var presetName = ""
    @State private var editingPresetIdx: Int?
    @State private var editingName = ""
    @FocusState private var nameFocused: Bool

    var body: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 12) {

                HStack {
                    Text("Presets".uppercased())
                        .font(.caption2.weight(.semibold))
                        .tracking(1.2)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Button {
                        ble.sendListPresets()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                            .font(.callout)
                    }
                    .buttonStyle(.glass)
                    .controlSize(.small)
                    .disabled(!radio.isConnected)
                }

                // Save row
                HStack(spacing: 8) {
                    TextField("Preset name", text: $presetName)
                        .textFieldStyle(.plain)
                        .padding(.horizontal, 12)
                        .padding(.vertical, 10)
                        .glassEffect(.regular, in: .rect(cornerRadius: 12))
                        .focused($nameFocused)

                    Button {
                        let name = presetName.isEmpty ? "Preset" : presetName
                        ble.sendSavePreset(name: String(name.prefix(19)))
                        presetName = ""
                        nameFocused = false
                    } label: {
                        Label("Save", systemImage: "square.and.arrow.down")
                    }
                    .buttonStyle(.glassProminent)
                    .tint(.green)
                    .disabled(!radio.isConnected || radio.scanData == nil)
                }

                if radio.presets.isEmpty {
                    Text("No presets saved. Run a scan and tap Save to store the channel list.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .padding(.vertical, 8)
                } else {
                    VStack(spacing: 8) {
                        ForEach(radio.presets) { preset in
                            PresetRow(
                                preset: preset,
                                isEditing: editingPresetIdx == preset.idx,
                                editingName: $editingName,
                                onTap: {
                                    editingPresetIdx = preset.idx
                                    editingName = preset.name
                                },
                                onSubmit: {
                                    let newName = String(editingName.prefix(19))
                                    if !newName.isEmpty, newName != preset.name {
                                        ble.sendRenamePreset(idx: preset.idx, name: newName)
                                    }
                                    editingPresetIdx = nil
                                },
                                onLoad: { ble.sendLoadPreset(idx: preset.idx) },
                                onDelete: { ble.sendDeletePreset(idx: preset.idx) }
                            )
                        }
                    }
                }
            }
        }
    }
}

private struct PresetRow: View {
    let preset: Preset
    let isEditing: Bool
    @Binding var editingName: String
    var onTap: () -> Void
    var onSubmit: () -> Void
    var onLoad: () -> Void
    var onDelete: () -> Void

    var body: some View {
        HStack(spacing: 8) {
            VStack(alignment: .leading, spacing: 1) {
                if isEditing {
                    TextField("Name", text: $editingName)
                        .font(.callout.monospaced())
                        .textFieldStyle(.plain)
                        .onSubmit(onSubmit)
                } else {
                    Text(preset.name)
                        .font(.callout.monospaced().weight(.medium))
                        .onTapGesture(perform: onTap)
                }
                Text("\(preset.channelCount) channels")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            GlassEffectContainer {
                HStack(spacing: 4) {
                    Button("Load", action: onLoad)
                        .buttonStyle(.glass)
                        .controlSize(.small)
                        .tint(.accent)

                    Button(role: .destructive, action: onDelete) {
                        Image(systemName: "trash")
                    }
                    .buttonStyle(.glass)
                    .controlSize(.small)
                }
            }
        }
        .padding(.vertical, 6)
        .padding(.horizontal, 10)
        .glassEffect(.regular, in: .rect(cornerRadius: 12))
    }
}
