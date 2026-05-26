import SwiftUI

struct LogCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                if radio.logMessages.isEmpty {
                    emptyState
                } else {
                    LazyVStack(alignment: .leading, spacing: 4) {
                        ForEach(radio.logMessages) { entry in
                            HStack(alignment: .top, spacing: 8) {
                                Text(entry.timestamp, format: .dateTime.hour().minute().second())
                                    .font(.caption2.monospaced())
                                    .foregroundStyle(.tertiary)
                                    .frame(width: 64, alignment: .leading)
                                Text(entry.message)
                                    .font(.caption.monospaced())
                                    .foregroundStyle(logColor(entry.type))
                                    .textSelection(.enabled)
                                Spacer(minLength: 0)
                            }
                            .id(entry.id)
                        }
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 12)
                }
            }
            .scrollIndicators(.hidden)
            .scrollContentBackground(.hidden)
            .onChange(of: radio.logMessages.count) { _, _ in
                if let last = radio.logMessages.last {
                    withAnimation(.smooth(duration: 0.2)) {
                        proxy.scrollTo(last.id, anchor: .bottom)
                    }
                }
            }
        }
    }

    private var emptyState: some View {
        VStack(spacing: 12) {
            Image(systemName: "text.alignleft")
                .font(.largeTitle)
                .foregroundStyle(.tertiary)
            Text("No log messages")
                .font(.subheadline.weight(.medium))
                .foregroundStyle(.secondary)
            Text("Connect to a device to start logging.")
                .font(.footnote)
                .foregroundStyle(.tertiary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding(.top, 80)
    }

    private func logColor(_ type: LogEntry.LogType) -> Color {
        switch type {
        case .info:  return .primary.opacity(0.85)
        case .ok:    return .green
        case .error: return .red
        }
    }
}
