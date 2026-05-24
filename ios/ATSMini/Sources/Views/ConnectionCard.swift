import SwiftUI

struct ConnectionCard: View {
    @EnvironmentObject var radio: RadioState
    @ObservedObject private var ble = BLEManager.shared
    @State private var showDevicePicker = false

    var body: some View {
        GlassCard {
            HStack {
                Circle()
                    .fill(radio.isConnected ? Color.green : Color.gray)
                    .frame(width: 8, height: 8)
                Text(radio.connectionStatus)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Spacer()
                if radio.isConnected {
                    Button("Disconnect") {
                        ble.disconnect()
                    }
                    .font(.caption)
                    .buttonStyle(.glass)
                    .tint(.red)
                } else {
                    Button("Connect") {
                        showDevicePicker = true
                        ble.startScan()
                    }
                    .font(.caption.bold())
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
    @EnvironmentObject var radio: RadioState

    var body: some View {
        NavigationStack {
            List {
                if ble.isScanning {
                    HStack {
                        ProgressView()
                            .padding(.trailing, 8)
                        Text("Scanning for devices...")
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
                                .foregroundStyle(Color.accent)
                            Text(device.name ?? device.identifier.uuidString)
                                .foregroundStyle(.primary)
                        }
                    }
                }
                if ble.discoveredDevices.isEmpty && !ble.isScanning {
                    Text("No devices found. Make sure your ATS-Mini has BLE enabled.")
                        .foregroundStyle(.secondary)
                        .font(.caption)
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
        .presentationDetents([.medium])
    }
}
