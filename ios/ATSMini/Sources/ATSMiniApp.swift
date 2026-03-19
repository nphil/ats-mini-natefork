import SwiftUI

@main
struct ATSMiniApp: App {
    @StateObject private var radio = RadioState()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(radio)
                .preferredColorScheme(.dark)
        }
    }
}
