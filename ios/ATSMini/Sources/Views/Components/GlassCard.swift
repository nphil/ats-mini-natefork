import SwiftUI

/// A frosted-glass card container using material effects
struct GlassCard<Content: View>: View {
    let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        content
            .padding(14)
            .background(.ultraThinMaterial)
            .clipShape(RoundedRectangle(cornerRadius: 16))
            .overlay(
                RoundedRectangle(cornerRadius: 16)
                    .strokeBorder(.white.opacity(0.1), lineWidth: 0.5)
            )
            .shadow(color: .black.opacity(0.2), radius: 8, y: 4)
    }
}
