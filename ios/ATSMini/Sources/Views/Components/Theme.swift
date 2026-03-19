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
