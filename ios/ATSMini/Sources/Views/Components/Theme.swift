import SwiftUI

// MARK: - HSL Color helper

extension Color {
    /// Build a Color from HSL components (matches HomeBoy's theme format).
    init(h: Double, s: Double, l: Double, opacity: Double = 1) {
        let H = h / 360.0
        let S = s / 100.0
        let L = l / 100.0

        let c = (1 - abs(2 * L - 1)) * S
        let hp = H * 6
        let x = c * (1 - abs(hp.truncatingRemainder(dividingBy: 2) - 1))
        let m = L - c / 2

        let (r1, g1, b1): (Double, Double, Double)
        switch hp {
        case 0..<1: (r1, g1, b1) = (c, x, 0)
        case 1..<2: (r1, g1, b1) = (x, c, 0)
        case 2..<3: (r1, g1, b1) = (0, c, x)
        case 3..<4: (r1, g1, b1) = (0, x, c)
        case 4..<5: (r1, g1, b1) = (x, 0, c)
        case 5..<6: (r1, g1, b1) = (c, 0, x)
        default:    (r1, g1, b1) = (0, 0, 0)
        }
        self = Color(red: r1 + m, green: g1 + m, blue: b1 + m, opacity: opacity)
    }
}

// MARK: - App Themes (ported from HomeBoy)

enum AppTheme: String, CaseIterable, Identifiable {
    case homebox, light, dark, forest, garden, emerald, aqua, ocean
    case night, dracula, synthwave, halloween, coffee, business, luxury, black
    case cupcake, valentine, pastel, fantasy, retro, bumblebee, lemonade
    case corporate, cmyk, autumn, winter, acid, cyberpunk, wireframe, lofi

    var id: String { rawValue }

    var displayName: String {
        rawValue.prefix(1).uppercased() + rawValue.dropFirst()
    }

    /// (hue, saturation, lightness) tuples for background, foreground, primary, accent.
    var hsl: (bg: (Double, Double, Double),
              fg: (Double, Double, Double),
              primary: (Double, Double, Double),
              accent: (Double, Double, Double)) {
        switch self {
        case .homebox:    return ((0,0,100),    (0,0,20),     (139,16,43),  (97,37,93))
        case .light:      return ((0,0,100),    (215,28,17),  (259,94,51),  (314,100,47))
        case .dark:       return ((0,0,11),     (0,0,90),     (259,94,70),  (314,100,70))
        case .forest:     return ((0,12,8),     (0,12,82),    (141,72,42),  (141,75,48))
        case .garden:     return ((0,4,91),     (0,3,6),      (139,16,43),  (97,37,93))
        case .emerald:    return ((0,0,100),    (219,20,25),  (141,50,60),  (219,96,60))
        case .aqua:       return ((219,53,43),  (218,100,89), (182,93,49),  (274,31,57))
        case .ocean:      return ((207,50,14),  (207,30,90),  (199,89,64),  (259,50,67))
        case .night:      return ((222,47,11),  (222,65,82),  (198,93,60),  (234,89,74))
        case .dracula:    return ((231,15,18),  (60,30,96),   (326,100,74), (265,89,78))
        case .synthwave:  return ((254,59,26),  (260,60,98),  (321,70,69),  (197,87,65))
        case .halloween:  return ((0,0,13),     (0,0,83),     (32,89,52),   (271,46,42))
        case .coffee:     return ((306,19,11),  (37,30,70),   (30,67,58),   (182,25,50))
        case .business:   return ((0,0,13),     (0,0,82),     (210,64,55),  (200,13,65))
        case .luxury:     return ((240,10,4),   (37,67,58),   (0,0,100),    (218,54,50))
        case .black:      return ((0,0,0),      (0,0,80),     (0,0,70),     (0,0,50))
        case .cupcake:    return ((24,33,97),   (280,46,14),  (183,47,59),  (338,71,78))
        case .valentine:  return ((318,46,89),  (344,38,28),  (353,74,67),  (254,86,77))
        case .pastel:     return ((0,0,100),    (0,0,20),     (284,22,70),  (352,70,80))
        case .fantasy:    return ((0,0,100),    (215,28,17),  (296,83,35),  (200,100,37))
        case .retro:      return ((45,47,80),   (345,5,15),   (3,60,55),    (145,35,50))
        case .bumblebee:  return ((0,0,100),    (0,0,20),     (41,74,53),   (50,94,58))
        case .lemonade:   return ((0,0,100),    (0,0,20),     (89,96,31),   (60,81,45))
        case .corporate:  return ((0,0,100),    (233,27,13),  (229,96,64),  (215,26,59))
        case .cmyk:       return ((0,0,100),    (0,0,20),     (203,83,60),  (335,78,60))
        case .autumn:     return ((0,0,95),     (0,0,19),     (344,96,38),  (0,63,50))
        case .winter:     return ((0,0,100),    (214,30,32),  (212,100,51), (247,47,43))
        case .acid:       return ((0,0,98),     (0,0,20),     (303,90,45),  (27,100,50))
        case .cyberpunk:  return ((56,100,50),  (56,100,10),  (345,100,50), (195,80,55))
        case .wireframe:  return ((0,0,100),    (0,0,20),     (0,0,40),     (0,0,60))
        case .lofi:       return ((0,0,100),    (0,0,0),      (0,0,5),      (0,0,30))
        }
    }

    var backgroundColor: Color { Color(h: hsl.bg.0,      s: hsl.bg.1,      l: hsl.bg.2) }
    var foregroundColor: Color { Color(h: hsl.fg.0,      s: hsl.fg.1,      l: hsl.fg.2) }
    var primaryColor:    Color { Color(h: hsl.primary.0, s: hsl.primary.1, l: hsl.primary.2) }
    var accentColor:     Color { Color(h: hsl.accent.0,  s: hsl.accent.1,  l: hsl.accent.2) }

    /// Light or dark mode based on background lightness.
    var colorScheme: ColorScheme { hsl.bg.2 > 50 ? .light : .dark }
}

// MARK: - Theme Store (persistence)

@MainActor
final class ThemeStore: ObservableObject {
    @Published private(set) var current: AppTheme {
        didSet { UserDefaults.standard.set(current.rawValue, forKey: Self.storageKey) }
    }

    private static let storageKey = "atsmini.theme"

    init() {
        if let raw = UserDefaults.standard.string(forKey: Self.storageKey),
           let saved = AppTheme(rawValue: raw) {
            current = saved
        } else {
            current = .night
        }
    }

    func set(_ theme: AppTheme) {
        withAnimation(.smooth(duration: 0.25)) { current = theme }
    }
}

// MARK: - Solid background, theme-aware

struct AppBackground: View {
    @EnvironmentObject var theme: ThemeStore

    var body: some View {
        theme.current.backgroundColor
            .ignoresSafeArea()
    }
}

// MARK: - Legacy color shims (kept for code that hasn't migrated to theme yet)

extension Color {
    /// Fallback accent — overridden at view scope by `.tint(theme.current.accentColor)`.
    static let accent = Color(red: 0, green: 0.83, blue: 1.0)
    static let lcdGreen = Color(red: 0, green: 1, blue: 0.25)
    static let lcdBackground = Color(red: 0.04, green: 0.1, blue: 0.04)
}

extension ShapeStyle where Self == Color {
    static var accent: Color { .accent }
}
