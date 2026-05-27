import SwiftUI
import UniformTypeIdentifiers

// MARK: - OTA Manager

final class OTAManager: NSObject, ObservableObject, URLSessionTaskDelegate {

    enum Phase: Equatable {
        case idle
        case downloading
        case uploading
        case done
        case error(String)

        var isActive: Bool {
            switch self { case .downloading, .uploading: return true; default: return false }
        }
    }

    @Published var phase: Phase = .idle
    @Published var progress: Double = 0
    @Published var statusMessage = ""

    private var uploadSession: URLSession?

    // MARK: Flash from local file

    func flashFromFile(_ fileURL: URL, host: String, ble: BLEManager) {
        guard fileURL.startAccessingSecurityScopedResource() else {
            set(.error("Cannot access file")); return
        }
        defer { fileURL.stopAccessingSecurityScopedResource() }

        guard let data = try? Data(contentsOf: fileURL) else {
            set(.error("Cannot read file")); return
        }
        upload(data: data, host: host, ble: ble)
    }

    // MARK: Flash from web URL

    func flashFromURL(_ urlString: String, host: String, ble: BLEManager) {
        guard let url = URL(string: urlString) else { set(.error("Invalid URL")); return }

        set(.downloading, message: "Downloading firmware…")

        URLSession.shared.dataTask(with: url) { [weak self] data, _, error in
            guard let self else { return }
            if let error { self.set(.error(error.localizedDescription)); return }
            guard let data else { self.set(.error("No data received")); return }
            self.upload(data: data, host: host, ble: ble)
        }.resume()
    }

    // MARK: Reset

    func reset() {
        uploadSession?.invalidateAndCancel()
        uploadSession = nil
        phase = .idle
        progress = 0
        statusMessage = ""
    }

    // MARK: Private

    private func upload(data: Data, host: String, ble: BLEManager) {
        let boundary = UUID().uuidString
        var body = Data()
        body.append("--\(boundary)\r\n".utf8Data)
        body.append("Content-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n".utf8Data)
        body.append("Content-Type: application/octet-stream\r\n\r\n".utf8Data)
        body.append(data)
        body.append("\r\n--\(boundary)--\r\n".utf8Data)

        guard let url = URL(string: "http://\(host)/update") else {
            set(.error("Invalid host")); return
        }

        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("multipart/form-data; boundary=\(boundary)", forHTTPHeaderField: "Content-Type")
        request.timeoutInterval = 120

        let session = URLSession(configuration: .default, delegate: self, delegateQueue: nil)
        uploadSession = session

        set(.uploading, message: "Uploading to device…")

        session.uploadTask(with: request, from: body) { [weak self] _, response, error in
            guard let self else { return }
            DispatchQueue.main.async {
                if let error {
                    self.phase = .error(error.localizedDescription)
                    return
                }
                let code = (response as? HTTPURLResponse)?.statusCode ?? 0
                if code == 200 {
                    self.phase = .done
                    self.statusMessage = "Update complete — reconnecting…"
                    self.scheduleReconnect(ble: ble)
                } else {
                    self.phase = .error("Upload failed (HTTP \(code))")
                }
            }
        }.resume()
    }

    func urlSession(_ session: URLSession, task: URLSessionTask,
                    didSendBodyData bytesSent: Int64,
                    totalBytesSent: Int64,
                    totalBytesExpectedToSend: Int64) {
        let p = Double(totalBytesSent) / Double(totalBytesExpectedToSend)
        DispatchQueue.main.async { self.progress = p }
    }

    private func scheduleReconnect(ble: BLEManager) {
        let name = ble.connectedPeripheralName
        DispatchQueue.main.asyncAfter(deadline: .now() + 7) {
            self.statusMessage = "Scanning for device…"
            ble.autoConnectName = name
            ble.startScan()
        }
    }

    private func set(_ newPhase: Phase, message: String = "") {
        DispatchQueue.main.async {
            self.phase = newPhase
            self.statusMessage = message
            if newPhase == .idle { self.progress = 0 }
        }
    }
}

private extension String {
    var utf8Data: Data { Data(utf8) }
}

// MARK: - Settings View

struct SettingsView: View {
    @EnvironmentObject var theme: ThemeStore
    @EnvironmentObject var radio: RadioState
    @StateObject private var ota = OTAManager()

    // Releases
    @State private var releases: [FirmwareRelease] = []
    @State private var releasesLoading = false
    @State private var releasesError: String? = nil

    // Manual / advanced
    @State private var showAdvanced = false
    @State private var manualHost = "atsmini.local"
    @State private var manualSource: ManualSource = .file
    @State private var firmwareURL = ""
    @State private var showFilePicker = false
    @State private var showThemePicker = false

    enum ManualSource: String, CaseIterable, Identifiable {
        case file = "Local File"
        case url  = "Web URL"
        var id: String { rawValue }
    }

    // Best host: prefer auto-detected IP from device status
    private var effectiveHost: String {
        radio.wifiIP.isEmpty ? manualHost : radio.wifiIP
    }

    var body: some View {
        Form {
            // MARK: Appearance
            Section {
                Button {
                    showThemePicker = true
                } label: {
                    HStack(spacing: 14) {
                        ThemeSwatch(theme: theme.current, isSelected: false, size: 32)
                            .allowsHitTesting(false)
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Theme")
                                .foregroundStyle(.primary)
                            Text(theme.current.displayName)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                        Image(systemName: "chevron.right")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(.tertiary)
                    }
                }
                .buttonStyle(.plain)
            } header: {
                Text("Appearance")
            } footer: {
                Text("30 color themes ported from Homebox / HomeBoy.")
            }

            // MARK: Firmware Update
            Section {
                deviceConnectionRow
                releasesContent
            } header: {
                HStack {
                    Text("Firmware Update")
                    Spacer()
                    if releasesLoading {
                        ProgressView().scaleEffect(0.7)
                    } else {
                        Button {
                            Task { await loadReleases() }
                        } label: {
                            Image(systemName: "arrow.clockwise")
                                .font(.caption)
                        }
                        .buttonStyle(.plain)
                        .foregroundStyle(.secondary)
                    }
                }
            } footer: {
                if radio.wifiIP.isEmpty {
                    Text("Enable Wi-Fi on the radio (or use AP mode) so the app can reach its update endpoint.")
                } else if radio.wifiIsAP {
                    Text("Device is in AP mode. Connect your phone to its Wi-Fi network, then flash.")
                } else {
                    Text("Connected via the same Wi-Fi network.")
                }
            }

            // MARK: OTA Progress (shown while/after flash)
            if ota.phase != .idle {
                Section {
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            statusIcon
                            Text(ota.statusMessage.isEmpty ? statusLabel : ota.statusMessage)
                                .font(.callout)
                                .foregroundStyle(statusColor)
                            Spacer()
                            if ota.phase.isActive {
                                Text("\(Int(ota.progress * 100))%")
                                    .font(.callout.monospacedDigit().weight(.medium))
                                    .foregroundStyle(.secondary)
                            }
                        }
                        ProgressView(value: progressValue)
                            .progressViewStyle(.linear)
                            .tint(progressTint)
                            .animation(.smooth(duration: 0.25), value: ota.progress)
                    }
                    .padding(.vertical, 4)

                    if !ota.phase.isActive {
                        Button("Dismiss", role: .cancel) { ota.reset() }
                            .frame(maxWidth: .infinity)
                    }
                } header: {
                    Text("Progress")
                }
            }

            // MARK: Advanced (manual flash)
            Section {
                Button {
                    withAnimation { showAdvanced.toggle() }
                } label: {
                    HStack {
                        Text("Manual Flash")
                            .foregroundStyle(.primary)
                        Spacer()
                        Image(systemName: showAdvanced ? "chevron.up" : "chevron.down")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(.tertiary)
                    }
                }
                .buttonStyle(.plain)

                if showAdvanced {
                    LabeledContent("Device host") {
                        TextField("atsmini.local or IP", text: $manualHost)
                            .multilineTextAlignment(.trailing)
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled()
                    }

                    Picker("Source", selection: $manualSource) {
                        ForEach(ManualSource.allCases) { Text($0.rawValue).tag($0) }
                    }
                    .pickerStyle(.segmented)
                    .disabled(ota.phase.isActive)

                    if manualSource == .url {
                        LabeledContent("URL") {
                            TextField("https://…/firmware.bin", text: $firmwareURL)
                                .multilineTextAlignment(.trailing)
                                .textInputAutocapitalization(.never)
                                .autocorrectionDisabled()
                                .keyboardType(.URL)
                        }
                    }

                    if manualSource == .file {
                        Button {
                            showFilePicker = true
                        } label: {
                            Label("Choose .bin file…", systemImage: "folder")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.glassProminent)
                        .tint(theme.current.accentColor)
                        .disabled(ota.phase.isActive)
                    } else {
                        Button {
                            ota.flashFromURL(firmwareURL, host: manualHost, ble: BLEManager.shared)
                        } label: {
                            Label("Flash from URL", systemImage: "arrow.down.circle")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.glassProminent)
                        .tint(theme.current.accentColor)
                        .disabled(ota.phase.isActive || firmwareURL.isEmpty)
                    }
                }
            } header: {
                Text("Advanced")
            }

            // MARK: About
            Section("About") {
                LabeledContent("App version", value: appVersion)
                LabeledContent("Build", value: appBuild)
                if radio.firmwareVersion > 0 {
                    LabeledContent("Radio firmware", value: formattedFirmwareVersion(radio.firmwareVersion))
                }
                Link(destination: URL(string: "https://github.com/nphil/ats-mini-natefork")!) {
                    Label("GitHub", systemImage: "link")
                }
            }
        }
        .formStyle(.grouped)
        .fileImporter(
            isPresented: $showFilePicker,
            allowedContentTypes: [UTType.data],
            allowsMultipleSelection: false
        ) { result in
            if case .success(let urls) = result, let url = urls.first {
                ota.flashFromFile(url, host: manualHost, ble: BLEManager.shared)
            }
        }
        .sheet(isPresented: $showThemePicker) {
            ThemePickerSheet(isPresented: $showThemePicker)
        }
        .task { await loadReleases() }
    }

    // MARK: - Device connection row

    @ViewBuilder
    private var deviceConnectionRow: some View {
        if !radio.wifiIP.isEmpty {
            HStack {
                Image(systemName: radio.wifiIsAP ? "wifi.router" : "wifi")
                    .foregroundStyle(theme.current.accentColor)
                VStack(alignment: .leading, spacing: 2) {
                    Text(radio.wifiIsAP ? "AP mode" : "Same network")
                        .font(.callout)
                    Text(radio.wifiIP)
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Image(systemName: "checkmark.circle.fill")
                    .foregroundStyle(.green)
            }
            .padding(.vertical, 2)
        } else {
            HStack {
                Image(systemName: "wifi.slash")
                    .foregroundStyle(.secondary)
                Text("Radio not on Wi-Fi")
                    .foregroundStyle(.secondary)
            }
        }
    }

    // MARK: - Releases content

    @ViewBuilder
    private var releasesContent: some View {
        if let error = releasesError {
            HStack {
                Image(systemName: "exclamationmark.triangle")
                    .foregroundStyle(.orange)
                Text(error)
                    .font(.callout)
                    .foregroundStyle(.secondary)
                Spacer()
                Button("Retry") { Task { await loadReleases() } }
                    .font(.callout)
                    .buttonStyle(.plain)
                    .foregroundStyle(theme.current.accentColor)
            }
        } else if releases.isEmpty && !releasesLoading {
            Text("No firmware releases found.")
                .font(.callout)
                .foregroundStyle(.secondary)
        } else {
            ForEach(Array(releases.enumerated()), id: \.element.id) { idx, release in
                releaseRow(release, isLatest: idx == 0)
            }
        }
    }

    @ViewBuilder
    private func releaseRow(_ release: FirmwareRelease, isLatest: Bool) -> some View {
        let isCurrent = radio.firmwareVersion > 0 && release.versionInt == radio.firmwareVersion
        let canFlash = !radio.wifiIP.isEmpty && !ota.phase.isActive

        HStack(alignment: .center, spacing: 10) {
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 6) {
                    Text("v\(release.version)")
                        .font(.callout.weight(.semibold))
                        .foregroundStyle(isCurrent ? theme.current.accentColor : .primary)
                    if isLatest {
                        Text("LATEST")
                            .font(.caption2.weight(.bold))
                            .padding(.horizontal, 5)
                            .padding(.vertical, 2)
                            .background(theme.current.accentColor.opacity(0.15))
                            .foregroundStyle(theme.current.accentColor)
                            .clipShape(RoundedRectangle(cornerRadius: 4))
                    }
                    if isCurrent {
                        Text("INSTALLED")
                            .font(.caption2.weight(.bold))
                            .padding(.horizontal, 5)
                            .padding(.vertical, 2)
                            .background(Color.green.opacity(0.15))
                            .foregroundStyle(.green)
                            .clipShape(RoundedRectangle(cornerRadius: 4))
                    }
                }
                Text("\(release.formattedDate) · \(release.formattedSize)")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Button {
                UIImpactFeedbackGenerator(style: .medium).impactOccurred()
                ota.flashFromURL(release.downloadURL, host: effectiveHost, ble: BLEManager.shared)
            } label: {
                Text(isCurrent ? "Re-flash" : "Flash")
                    .font(.callout.weight(.medium))
                    .padding(.horizontal, 12)
                    .padding(.vertical, 6)
            }
            .buttonStyle(.glass)
            .tint(theme.current.accentColor)
            .disabled(!canFlash)
        }
        .padding(.vertical, 2)
    }

    // MARK: - Load releases

    private func loadReleases() async {
        releasesLoading = true
        releasesError = nil
        do {
            let fetched = try await GitHubReleasesService.shared.fetchFirmwareReleases()
            await MainActor.run {
                releases = fetched
                releasesLoading = false
            }
        } catch {
            await MainActor.run {
                releasesError = "Could not load releases."
                releasesLoading = false
            }
        }
    }

    // MARK: - Helpers

    private var appVersion: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "—"
    }

    private var appBuild: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "—"
    }

    private func formattedFirmwareVersion(_ v: Int) -> String {
        "v\(v / 100).\(String(format: "%02d", v % 100))"
    }

    // MARK: - Status helpers

    @ViewBuilder
    private var statusIcon: some View {
        switch ota.phase {
        case .downloading: Image(systemName: "arrow.down.circle").foregroundStyle(statusColor)
        case .uploading:   Image(systemName: "arrow.up.circle").foregroundStyle(statusColor)
        case .done:        Image(systemName: "checkmark.circle.fill").foregroundStyle(statusColor)
        case .error:       Image(systemName: "xmark.circle.fill").foregroundStyle(statusColor)
        case .idle:        Image(systemName: "circle").foregroundStyle(statusColor)
        }
    }

    private var statusLabel: String {
        switch ota.phase {
        case .downloading: return "Downloading…"
        case .uploading:   return "Uploading…"
        case .done:        return "Update complete — device restarting"
        case .error(let m): return m
        case .idle:        return ""
        }
    }

    private var statusColor: Color {
        switch ota.phase {
        case .done:  return .green
        case .error: return .red
        default:     return .secondary
        }
    }

    private var progressValue: Double {
        ota.phase == .done ? 1.0 : ota.progress
    }

    private var progressTint: Color {
        switch ota.phase {
        case .done:  return .green
        case .error: return .red
        default:     return theme.current.accentColor
        }
    }
}

// MARK: - Theme Picker Sheet + Swatch

struct ThemePickerSheet: View {
    @Binding var isPresented: Bool
    @EnvironmentObject var theme: ThemeStore

    private let columns = Array(repeating: GridItem(.flexible(), spacing: 12), count: 4)

    var body: some View {
        NavigationStack {
            ZStack {
                AppBackground()
                ScrollView {
                    LazyVGrid(columns: columns, spacing: 18) {
                        ForEach(AppTheme.allCases) { t in
                            ThemeSwatchButton(theme: t,
                                              isSelected: theme.current == t) {
                                UIImpactFeedbackGenerator(style: .light).impactOccurred()
                                theme.set(t)
                            }
                        }
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 14)
                }
                .scrollIndicators(.hidden)
                .scrollContentBackground(.hidden)
            }
            .navigationTitle("Choose Theme")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { isPresented = false }
                }
            }
        }
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
    }
}

struct ThemeSwatchButton: View {
    let theme: AppTheme
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            VStack(spacing: 6) {
                ThemeSwatch(theme: theme, isSelected: isSelected, size: 56)
                Text(theme.displayName)
                    .font(.caption2.weight(.medium))
                    .foregroundStyle(.primary)
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
            }
        }
        .buttonStyle(.plain)
    }
}

struct ThemeSwatch: View {
    let theme: AppTheme
    let isSelected: Bool
    var size: CGFloat = 48

    var body: some View {
        ZStack {
            Circle()
                .fill(theme.backgroundColor)
                .frame(width: size, height: size)
                .overlay {
                    Circle().stroke(.secondary.opacity(0.25), lineWidth: 1)
                }

            Circle()
                .fill(theme.primaryColor)
                .frame(width: size * 0.46, height: size * 0.46)
                .offset(x: -size * 0.13, y: -size * 0.04)

            Circle()
                .fill(theme.accentColor)
                .frame(width: size * 0.29, height: size * 0.29)
                .offset(x: size * 0.19, y: size * 0.13)

            if isSelected {
                Circle()
                    .stroke(theme.accentColor, lineWidth: 3)
                    .frame(width: size + 6, height: size + 6)
            }
        }
        .frame(width: size + 8, height: size + 8)
    }
}
