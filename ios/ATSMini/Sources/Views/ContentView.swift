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
        }
        .tint(Color.accent)
        .onAppear {
            ble.radio = radio
        }
    }
}

// MARK: - Radio Tab

struct RadioTab: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        NavigationStack {
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
                .padding()
            }
            .background(Color(.systemGroupedBackground))
            .navigationTitle("ATS-Mini")
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

// MARK: - Spectrum Tab

struct SpectrumTab: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 16) {
                    SpectrumCard()
                    PresetsCard()
                }
                .padding()
            }
            .background(Color(.systemGroupedBackground))
            .navigationTitle("Spectrum")
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

// MARK: - Waterfall Tab

struct WaterfallTab: View {
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 16) {
                    WaterfallCard()
                }
                .padding()
            }
            .background(Color(.systemGroupedBackground))
            .navigationTitle("Waterfall")
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

// MARK: - Log Tab

struct LogTab: View {
    var body: some View {
        NavigationStack {
            LogCard()
                .background(Color(.systemGroupedBackground))
                .navigationTitle("Log")
                .navigationBarTitleDisplayMode(.inline)
        }
    }
}
