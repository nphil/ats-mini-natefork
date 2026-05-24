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

// MARK: - Settings / Firmware Update View

struct FirmwareUpdateView: View {
    @StateObject private var ota = OTAManager()

    enum Source: String, CaseIterable, Identifiable {
        case file = "Local File"
        case url  = "Web URL"
        var id: String { rawValue }
    }

    @State private var source: Source = .file
    @State private var host = "atsmini.local"
    @State private var firmwareURL = ""
    @State private var showFilePicker = false

    var body: some View {
        Form {
            Section {
                LabeledContent("Device") {
                    TextField("atsmini.local or IP", text: $host)
                        .multilineTextAlignment(.trailing)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                }
            } header: {
                Text("Connection")
            } footer: {
                Text("Phone must be on the same Wi-Fi network as the device (or connected to the device's AP).")
            }

            Section("Source") {
                Picker("Update from", selection: $source) {
                    ForEach(Source.allCases) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                .disabled(ota.phase.isActive)

                if source == .url {
                    LabeledContent("URL") {
                        TextField("https://…/firmware.bin", text: $firmwareURL)
                            .multilineTextAlignment(.trailing)
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled()
                            .keyboardType(.URL)
                    }
                }
            }

            Section {
                if source == .file {
                    Button {
                        showFilePicker = true
                    } label: {
                        Label("Choose .bin file…", systemImage: "folder")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.glassProminent)
                    .tint(.accent)
                    .disabled(ota.phase.isActive)
                } else {
                    Button {
                        ota.flashFromURL(firmwareURL, host: host, ble: BLEManager.shared)
                    } label: {
                        Label("Flash from URL", systemImage: "arrow.down.circle")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.glassProminent)
                    .tint(.accent)
                    .disabled(ota.phase.isActive || firmwareURL.isEmpty)
                }

                if ota.phase != .idle {
                    Button("Reset", role: .cancel) { ota.reset() }
                        .frame(maxWidth: .infinity)
                        .disabled(ota.phase.isActive)
                }
            }

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
                } header: {
                    Text("Progress")
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
                ota.flashFromFile(url, host: host, ble: BLEManager.shared)
            }
        }
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
        default:     return .accent
        }
    }
}
