import SwiftUI

extension Color {
    /// Accent color: cyan-ish (#00D4FF)
    static let accent = Color(red: 0, green: 0.83, blue: 1.0)
    /// LCD green
    static let lcdGreen = Color(red: 0, green: 1, blue: 0.25)
    /// LCD background
    static let lcdBackground = Color(red: 0.04, green: 0.1, blue: 0.04)
}

extension ShapeStyle where Self == Color {
    static var accent: Color { .accent }
}

/// Full-screen gradient backdrop — sits behind TabView, lets glass cards float over it.
struct AppBackground: View {
    var body: some View {
        LinearGradient(
            colors: [
                Color(red: 0.05, green: 0.08, blue: 0.13),
                Color(red: 0.02, green: 0.04, blue: 0.08),
                Color(red: 0.04, green: 0.07, blue: 0.10)
            ],
            startPoint: .topLeading,
            endPoint: .bottomTrailing
        )
        .ignoresSafeArea()
        .overlay(alignment: .top) {
            RadialGradient(
                colors: [Color.accent.opacity(0.18), .clear],
                center: .top,
                startRadius: 5,
                endRadius: 320
            )
            .ignoresSafeArea()
            .allowsHitTesting(false)
        }
    }
}
