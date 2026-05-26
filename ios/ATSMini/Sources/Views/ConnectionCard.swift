import SwiftUI

// MARK: - Toolbar Connection Button

struct ConnectionStatusButton: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var showPicker = false

    var body: some View {
        Button {
            if radio.isConnected {
                showPicker = true
            } else {
                showPicker = true
                ble.startScan()
            }
        } label: {
            HStack(spacing: 6) {
                connectionDot
                Text(radio.isConnected ? (radio.connectionStatus) : "Connect")
                    .font(.subheadline.weight(.medium))
                    .lineLimit(1)
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 5)
            .glassEffect(.regular, in: .capsule)
        }
        .buttonStyle(.plain)
        .sheet(isPresented: $showPicker, onDismiss: {
            if !radio.isConnected { ble.stopScan() }
        }) {
            DevicePickerSheet(isPresented: $showPicker)
        }
    }

    @ViewBuilder
    private var connectionDot: some View {
        ZStack {
            if radio.isConnected {
                Circle()
                    .fill(Color.green.opacity(0.35))
                    .frame(width: 14, height: 14)
            }
            Circle()
                .fill(radio.isConnected ? Color.green : Color.secondary.opacity(0.6))
                .frame(width: 8, height: 8)
        }
    }
}

// MARK: - Device Picker Sheet

struct DevicePickerSheet: View {
    @Binding var isPresented: Bool
    @ObservedObject private var ble = BLEManager.shared
    @EnvironmentObject var radio: RadioState

    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                List {
                    if radio.isConnected {
                        Section {
                            connectedRow
                        } header: {
                            Text("Connected")
                        }
                    }

                    Section {
                        if ble.isScanning {
                            HStack(spacing: 12) {
                                ProgressView().controlSize(.small)
                                Text("Scanning…")
                                    .foregroundStyle(.secondary)
                            }
                        }

                        ForEach(ble.discoveredDevices, id: \.identifier) { device in
                            Button {
                                UIImpactFeedbackGenerator(style: .medium).impactOccurred()
                                ble.connect(to: device)
                                isPresented = false
                            } label: {
                                HStack(spacing: 14) {
                                    Image(systemName: "antenna.radiowaves.left.and.right")
                                        .font(.callout)
                                        .foregroundStyle(.accent)
                                        .frame(width: 28)
                                    Text(device.name ?? device.identifier.uuidString)
                                        .foregroundStyle(.primary)
                                    Spacer()
                                    Image(systemName: "chevron.right")
                                        .font(.caption)
                                        .foregroundStyle(.tertiary)
                                }
                            }
                        }

                        if ble.discoveredDevices.isEmpty && !ble.isScanning {
                            Text("No devices found. Make sure your ATS-Mini has BLE enabled.")
                                .foregroundStyle(.secondary)
                                .font(.callout)
                                .padding(.vertical, 4)
                        }
                    } header: {
                        Text("Available Devices")
                    }
                }
                .scrollIndicators(.hidden)
                .scrollContentBackground(.hidden)
            }
            .navigationTitle("Bluetooth")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { isPresented = false }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        UIImpactFeedbackGenerator(style: .light).impactOccurred()
                        ble.startScan()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .disabled(ble.isScanning)
                }
            }
        }
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
    }

    @ViewBuilder
    private var connectedRow: some View {
        HStack(spacing: 14) {
            ZStack {
                Circle()
                    .fill(Color.green.opacity(0.2))
                    .frame(width: 28, height: 28)
                Circle()
                    .fill(Color.green)
                    .frame(width: 9, height: 9)
            }
            VStack(alignment: .leading, spacing: 2) {
                Text(radio.connectionStatus)
                    .font(.subheadline.weight(.medium))
                Text("Tap Disconnect to release")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            Button("Disconnect", role: .destructive) {
                UIImpactFeedbackGenerator(style: .medium).impactOccurred()
                ble.disconnect()
                isPresented = false
            }
            .buttonStyle(.glass)
            .controlSize(.small)
        }
    }
}
