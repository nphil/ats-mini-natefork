import SwiftUI

// MARK: - Toolbar Connection Button

struct ConnectionStatusButton: View {
    @EnvironmentObject var radio: RadioState
    @EnvironmentObject var theme: ThemeStore
    @ObservedObject private var ble = BLEManager.shared
    @State private var showPicker = false
    @State private var pulse = false

    var body: some View {
        Button {
            UIImpactFeedbackGenerator(style: .light).impactOccurred()
            showPicker = true
            if !radio.isConnected { ble.startScan() }
        } label: {
            ZStack {
                if radio.isConnected {
                    Circle()
                        .stroke(Color.green.opacity(0.6), lineWidth: 1.5)
                        .frame(width: 34, height: 34)
                        .scaleEffect(pulse ? 1.15 : 1.0)
                        .opacity(pulse ? 0 : 1)
                        .animation(
                            .easeOut(duration: 1.4).repeatForever(autoreverses: false),
                            value: pulse
                        )
                }

                Image(systemName: bluetoothIcon)
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundStyle(iconForeground)
                    .frame(width: 34, height: 34)
                    .background {
                        Circle()
                            .fill(.thinMaterial)
                            .overlay(Circle().stroke(borderColor, lineWidth: 1))
                    }
            }
        }
        .buttonStyle(.plain)
        .accessibilityLabel(radio.isConnected ? "Connected to \(radio.connectionStatus)" : "Connect via Bluetooth")
        .sheet(isPresented: $showPicker, onDismiss: {
            if !radio.isConnected { ble.stopScan() }
        }) {
            DevicePickerSheet(isPresented: $showPicker)
        }
        .onAppear { pulse = true }
    }

    private var bluetoothIcon: String {
        if radio.isConnected { return "dot.radiowaves.left.and.right" }
        return "antenna.radiowaves.left.and.right.slash"
    }

    private var iconForeground: Color {
        radio.isConnected ? .green : theme.current.accentColor
    }

    private var borderColor: Color {
        radio.isConnected
            ? Color.green.opacity(0.5)
            : theme.current.accentColor.opacity(0.45)
    }
}

// MARK: - Device Picker Sheet

struct DevicePickerSheet: View {
    @Binding var isPresented: Bool
    @ObservedObject private var ble = BLEManager.shared
    @EnvironmentObject var radio: RadioState
    @EnvironmentObject var theme: ThemeStore

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
                                        .foregroundStyle(theme.current.accentColor)
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
