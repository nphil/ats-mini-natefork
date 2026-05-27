import SwiftUI

@main
struct ATSMiniApp: App {
    @StateObject private var radio = RadioState()
    @StateObject private var theme = ThemeStore()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(radio)
                .environmentObject(theme)
                .tint(theme.current.accentColor)
                .preferredColorScheme(theme.current.colorScheme)
        }
    }
}
