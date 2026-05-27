import Foundation

// MARK: - Model

struct FirmwareRelease: Identifiable {
    let id = UUID()
    let tagName: String
    let publishedAt: Date
    let downloadURL: String
    let sizeBytes: Int
    let releaseNotes: String

    var version: String {
        tagName.hasPrefix("v") ? String(tagName.dropFirst()) : tagName
    }

    var formattedDate: String {
        let f = DateFormatter()
        f.dateStyle = .medium
        f.timeStyle = .none
        return f.string(from: publishedAt)
    }

    var formattedSize: String {
        let mb = Double(sizeBytes) / 1_048_576
        return String(format: "%.1f MB", mb)
    }

    // e.g. "235" for tag "v2.35"
    var versionInt: Int {
        let digits = version.filter { $0.isNumber }
        return Int(digits) ?? 0
    }
}

// MARK: - Service

actor GitHubReleasesService {
    static let shared = GitHubReleasesService()
    private init() {}

    private let releasesURL = URL(string: "https://api.github.com/repos/nphil/ats-mini-natefork/releases?per_page=20")!

    func fetchFirmwareReleases() async throws -> [FirmwareRelease] {
        var request = URLRequest(url: releasesURL)
        request.setValue("application/vnd.github.v3+json", forHTTPHeaderField: "Accept")
        request.setValue("ATSMini-iOS", forHTTPHeaderField: "User-Agent")
        request.cachePolicy = .reloadRevalidatingCacheData
        request.timeoutInterval = 15

        let (data, _) = try await URLSession.shared.data(for: request)
        let raw = try JSONDecoder().decode([GitHubRelease].self, from: data)

        let iso = ISO8601DateFormatter()
        return raw.compactMap { release -> FirmwareRelease? in
            let tag = release.tag_name
            // Only firmware releases: tag starts with "v" + digit, not prerelease
            guard tag.hasPrefix("v"),
                  tag.count > 1,
                  tag.dropFirst().first?.isNumber == true,
                  !release.prerelease,
                  let asset = release.assets.first(where: { $0.name.hasSuffix("-ospi-flash.bin") }),
                  let date = iso.date(from: release.published_at)
            else { return nil }

            return FirmwareRelease(
                tagName: tag,
                publishedAt: date,
                downloadURL: asset.browser_download_url,
                sizeBytes: asset.size,
                releaseNotes: release.body ?? ""
            )
        }
        .sorted { $0.publishedAt > $1.publishedAt }
    }

    // MARK: Private decodables

    private struct GitHubRelease: Decodable {
        let tag_name: String
        let published_at: String
        let body: String?
        let assets: [GitHubAsset]
        let prerelease: Bool
    }

    private struct GitHubAsset: Decodable {
        let name: String
        let browser_download_url: String
        let size: Int
    }
}
