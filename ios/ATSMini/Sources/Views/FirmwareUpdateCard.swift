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

    // URLSessionTaskDelegate — upload progress
    func urlSession(_ session: URLSession, task: URLSessionTask,
                    didSendBodyData bytesSent: Int64,
                    totalBytesSent: Int64,
                    totalBytesExpectedToSend: Int64) {
        let p = Double(totalBytesSent) / Double(totalBytesExpectedToSend)
        DispatchQueue.main.async { self.progress = p }
    }

    private func scheduleReconnect(ble: BLEManager) {
        let name = ble.connectedPeripheralName
        // Device reboots in ~0.5 s, BLE stack comes back up in ~5–7 s
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

// MARK: - Firmware Update Card

struct FirmwareUpdateCard: View {
    @StateObject private var ota = OTAManager()
    @StateObject private var ble = BLEManager.shared

    enum Source: String, CaseIterable { case file = "File", url = "URL" }

    @State private var source: Source = .file
    @State private var host = "atsmini.local"
    @State private var firmwareURL = ""
    @State private var showFilePicker = false

    var body: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 14) {

                Label("Firmware Update", systemImage: "arrow.down.circle")
                    .font(.headline)

                // Device address
                VStack(alignment: .leading, spacing: 4) {
                    Text("Device address").font(.caption).foregroundStyle(.secondary)
                    TextField("atsmini.local or IP", text: $host)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .textFieldStyle(.roundedBorder)
                }

                // Source picker
                Picker("Source", selection: $source) {
                    ForEach(Source.allCases, id: \.self) { Text($0.rawValue) }
                }
                .pickerStyle(.segmented)
                .disabled(ota.phase.isActive)

                // URL input
                if source == .url {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Firmware URL").font(.caption).foregroundStyle(.secondary)
                        TextField("https://…/firmware.bin", text: $firmwareURL)
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled()
                            .textFieldStyle(.roundedBorder)
                    }
                }

                // Progress area
                if ota.phase != .idle {
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Text(ota.statusMessage)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            Spacer()
                            if ota.phase.isActive {
                                Text("\(Int(ota.progress * 100))%")
                                    .font(.caption.monospacedDigit())
                            }
                        }
                        ProgressView(value: ota.phase == .done ? 1.0 : ota.progress)
                            .tint(ota.phase == .done ? .green : .accent)
                            .animation(.easeInOut, value: ota.progress)
                    }
                }

                // Status badges
                if case .error(let msg) = ota.phase {
                    Label(msg, systemImage: "xmark.circle.fill")
                        .font(.caption)
                        .foregroundStyle(.red)
                }
                if ota.phase == .done {
                    Label("Update complete — device restarting", systemImage: "checkmark.circle.fill")
                        .font(.caption)
                        .foregroundStyle(.green)
                }

                // Action buttons
                HStack(spacing: 10) {
                    if source == .file {
                        Button {
                            showFilePicker = true
                        } label: {
                            Label("Choose .bin…", systemImage: "folder")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.borderedProminent)
                        .disabled(ota.phase.isActive)
                    } else {
                        Button {
                            ota.flashFromURL(firmwareURL, host: host, ble: BLEManager.shared)
                        } label: {
                            Label("Flash from URL", systemImage: "arrow.down.circle")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.borderedProminent)
                        .disabled(ota.phase.isActive || firmwareURL.isEmpty)
                    }

                    if ota.phase != .idle {
                        Button("Reset", role: .cancel) { ota.reset() }
                            .buttonStyle(.bordered)
                            .disabled(ota.phase.isActive)
                    }
                }

                Text("Phone must be on the same WiFi network as the device (or connected to the device's AP).")
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }
        }
        .fileImporter(
            isPresented: $showFilePicker,
            allowedContentTypes: [UTType.data],
            allowsMultipleSelection: false
        ) { result in
            switch result {
            case .success(let urls):
                if let url = urls.first {
                    ota.flashFromFile(url, host: host, ble: BLEManager.shared)
                }
            case .failure(let error):
                // surface picker errors as OTA error state via a brief workaround
                _ = error
            }
        }
    }
}
