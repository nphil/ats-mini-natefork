import SwiftUI

struct ContentView: View {
    @EnvironmentObject var radio: RadioState
    @StateObject private var ble = BLEManager.shared

    var body: some View {
        TabView {
            Tab("Radio", systemImage: "radio") {
                RadioTab()
            }
            Tab("Visualize", systemImage: "waveform.path") {
                VisualizeTab()
            }
            Tab("Settings", systemImage: "gearshape") {
                SettingsTab()
            }
        }
        .tint(.accent)
        .onAppear { ble.radio = radio }
    }
}

// MARK: - Radio Tab

struct RadioTab: View {
    @EnvironmentObject var radio: RadioState
    @State private var showLog = false

    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                ScrollView {
                    VStack(spacing: 16) {
                        FrequencyCard()
                        ControlsCard()
                        SignalCard()
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 12)
                }
                .scrollIndicators(.hidden)
                .scrollContentBackground(.hidden)
            }
            .navigationTitle("ATS-Mini")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    ConnectionStatusButton()
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        showLog = true
                    } label: {
                        Image(systemName: "text.alignleft")
                            .symbolRenderingMode(.hierarchical)
                    }
                    .buttonStyle(.glass)
                    .controlSize(.small)
                }
            }
        }
        .sheet(isPresented: $showLog) {
            LogSheet()
        }
    }
}

// MARK: - Visualize Tab

struct VisualizeTab: View {
    enum Mode: String, CaseIterable {
        case spectrum = "Spectrum"
        case waterfall = "Waterfall"
    }

    @State private var mode: Mode = .spectrum

    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                ScrollView {
                    VStack(spacing: 16) {
                        Picker("View", selection: $mode) {
                            ForEach(Mode.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                        }
                        .pickerStyle(.segmented)
                        .padding(.horizontal, 16)
                        .padding(.top, 4)

                        if mode == .spectrum {
                            SpectrumCard()
                            PresetsCard()
                        } else {
                            WaterfallCard()
                        }
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 12)
                    .padding(.top, 0)
                }
                .scrollIndicators(.hidden)
                .scrollContentBackground(.hidden)
            }
            .navigationTitle("Visualize")
            .navigationBarTitleDisplayMode(.large)
        }
    }
}

// MARK: - Settings Tab

struct SettingsTab: View {
    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                FirmwareUpdateView()
                    .scrollIndicators(.hidden)
                    .scrollContentBackground(.hidden)
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.large)
        }
    }
}

// MARK: - Log Sheet

struct LogSheet: View {
    @EnvironmentObject var radio: RadioState
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                LogCard()
            }
            .navigationTitle("Log")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { dismiss() }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button(role: .destructive) {
                        radio.logMessages.removeAll()
                    } label: {
                        Image(systemName: "trash")
                    }
                    .disabled(radio.logMessages.isEmpty)
                }
            }
        }
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
    }
}
