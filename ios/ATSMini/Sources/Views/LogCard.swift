import SwiftUI

struct LogCard: View {
    @EnvironmentObject var radio: RadioState

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 2) {
                    ForEach(radio.logMessages) { entry in
                        HStack(alignment: .top, spacing: 6) {
                            Text(entry.timestamp, format: .dateTime.hour().minute().second())
                                .font(.system(size: 10, design: .monospaced))
                                .foregroundStyle(.secondary)
                            Text(entry.message)
                                .font(.system(size: 11, design: .monospaced))
                                .foregroundStyle(logColor(entry.type))
                        }
                        .id(entry.id)
                    }
                }
                .padding()
            }
            .background(Color(red: 0.04, green: 0.1, blue: 0.04))
            .onChange(of: radio.logMessages.count) { _, _ in
                if let last = radio.logMessages.last {
                    withAnimation {
                        proxy.scrollTo(last.id, anchor: .bottom)
                    }
                }
            }
        }
    }

    private func logColor(_ type: LogEntry.LogType) -> Color {
        switch type {
        case .info: return Color(red: 0.33, green: 0.53, blue: 0.33)
        case .ok: return .green
        case .error: return .red
        }
    }
}
