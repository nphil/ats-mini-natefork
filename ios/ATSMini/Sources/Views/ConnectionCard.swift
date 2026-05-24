import SwiftUI

struct ConnectionCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var showDevicePicker = false

    var body: some View {
        GlassCard(padding: 14) {
            HStack(spacing: 12) {
                ZStack {
                    Circle()
                        .fill(radio.isConnected ? Color.green : Color.gray)
                        .frame(width: 10, height: 10)
                    if radio.isConnected {
                        Circle()
                            .stroke(Color.green.opacity(0.4), lineWidth: 4)
                            .frame(width: 18, height: 18)
                    }
                }

                VStack(alignment: .leading, spacing: 1) {
                    Text(radio.isConnected ? "Connected" : "Not Connected")
                        .font(.subheadline.weight(.medium))
                    Text(radio.connectionStatus)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Spacer()

                if radio.isConnected {
                    Button("Disconnect", role: .destructive) { ble.disconnect() }
                        .buttonStyle(.glass)
                        .controlSize(.regular)
                } else {
                    Button {
                        showDevicePicker = true
                        ble.startScan()
                    } label: {
                        Label("Connect", systemImage: "antenna.radiowaves.left.and.right")
                            .labelStyle(.titleAndIcon)
                    }
                    .buttonStyle(.glassProminent)
                    .tint(.accent)
                }
            }
        }
        .sheet(isPresented: $showDevicePicker) {
            DevicePickerSheet(isPresented: $showDevicePicker)
        }
    }
}

struct DevicePickerSheet: View {
    @Binding var isPresented: Bool
    @ObservedObject private var ble = BLEManager.shared

    var body: some View {
        NavigationStack {
            List {
                Section {
                    if ble.isScanning {
                        HStack(spacing: 12) {
                            ProgressView().controlSize(.small)
                            Text("Scanning for devices…")
                                .foregroundStyle(.secondary)
                        }
                    }
                    ForEach(ble.discoveredDevices, id: \.identifier) { device in
                        Button {
                            ble.connect(to: device)
                            isPresented = false
                        } label: {
                            HStack {
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                    .foregroundStyle(.accent)
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
                    }
                } header: {
                    Text("Available Devices")
                }
            }
            .navigationTitle("Select Device")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { isPresented = false }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        ble.startScan()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
        }
        .presentationDetents([.medium, .large])
    }
}
