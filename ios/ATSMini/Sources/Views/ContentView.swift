import SwiftUI

struct ContentView: View {
    @EnvironmentObject var radio: RadioState
    @StateObject private var ble = BLEManager.shared

    var body: some View {
        TabView {
            Tab("Radio", systemImage: "radio") {
                RadioTab()
            }
            Tab("Spectrum", systemImage: "waveform.path") {
                SpectrumTab()
            }
            Tab("Waterfall", systemImage: "water.waves") {
                WaterfallTab()
            }
            Tab("Log", systemImage: "text.alignleft") {
                LogTab()
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

    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                ScrollView {
                    VStack(spacing: 16) {
                        ConnectionCard()
                        FrequencyCard()
                        if radio.isFM && (!radio.rdsStation.isEmpty || !radio.rdsPTY.isEmpty || !radio.rdsText.isEmpty) {
                            RDSCard()
                        }
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
        }
    }
}

// MARK: - Spectrum Tab

struct SpectrumTab: View {
    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                ScrollView {
                    VStack(spacing: 16) {
                        SpectrumCard()
                        PresetsCard()
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 12)
                }
                .scrollIndicators(.hidden)
                .scrollContentBackground(.hidden)
            }
            .navigationTitle("Spectrum")
            .navigationBarTitleDisplayMode(.large)
        }
    }
}

// MARK: - Waterfall Tab

struct WaterfallTab: View {
    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                ScrollView {
                    VStack(spacing: 16) {
                        WaterfallCard()
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 12)
                }
                .scrollIndicators(.hidden)
                .scrollContentBackground(.hidden)
            }
            .navigationTitle("Waterfall")
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

// MARK: - Log Tab

struct LogTab: View {
    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                LogCard()
            }
            .navigationTitle("Log")
            .navigationBarTitleDisplayMode(.large)
        }
    }
}
