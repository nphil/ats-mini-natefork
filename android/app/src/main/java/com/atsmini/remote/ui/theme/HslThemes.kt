package com.atsmini.remote.ui.theme

import androidx.compose.material3.ColorScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.ui.graphics.Color
import kotlin.math.abs

/** Build a Color from HSL components (matches the iOS/HomeBoy theme format). */
fun hsl(h: Double, s: Double, l: Double): Color {
    val hh = h / 360.0
    val ss = s / 100.0
    val ll = l / 100.0
    val c = (1 - abs(2 * ll - 1)) * ss
    val hp = hh * 6
    val x = c * (1 - abs(hp.mod(2.0) - 1))
    val m = ll - c / 2
    val (r, g, b) = when {
        hp < 1 -> Triple(c, x, 0.0)
        hp < 2 -> Triple(x, c, 0.0)
        hp < 3 -> Triple(0.0, c, x)
        hp < 4 -> Triple(0.0, x, c)
        hp < 5 -> Triple(x, 0.0, c)
        else -> Triple(c, 0.0, x)
    }
    return Color((r + m).toFloat(), (g + m).toFloat(), (b + m).toFloat())
}

/** The 30 HomeBoy palettes, identical to the iOS app for cross-device parity. */
enum class AppTheme(
    val displayName: String,
    private val bg: Triple<Double, Double, Double>,
    private val fg: Triple<Double, Double, Double>,
    private val primary: Triple<Double, Double, Double>,
    private val accent: Triple<Double, Double, Double>,
) {
    HOMEBOX("Homebox", Triple(0.0, 0.0, 100.0), Triple(0.0, 0.0, 20.0), Triple(139.0, 16.0, 43.0), Triple(97.0, 37.0, 93.0)),
    LIGHT("Light", Triple(0.0, 0.0, 100.0), Triple(215.0, 28.0, 17.0), Triple(259.0, 94.0, 51.0), Triple(314.0, 100.0, 47.0)),
    DARK("Dark", Triple(0.0, 0.0, 11.0), Triple(0.0, 0.0, 90.0), Triple(259.0, 94.0, 70.0), Triple(314.0, 100.0, 70.0)),
    FOREST("Forest", Triple(0.0, 12.0, 8.0), Triple(0.0, 12.0, 82.0), Triple(141.0, 72.0, 42.0), Triple(141.0, 75.0, 48.0)),
    GARDEN("Garden", Triple(0.0, 4.0, 91.0), Triple(0.0, 3.0, 6.0), Triple(139.0, 16.0, 43.0), Triple(97.0, 37.0, 93.0)),
    EMERALD("Emerald", Triple(0.0, 0.0, 100.0), Triple(219.0, 20.0, 25.0), Triple(141.0, 50.0, 60.0), Triple(219.0, 96.0, 60.0)),
    AQUA("Aqua", Triple(219.0, 53.0, 43.0), Triple(218.0, 100.0, 89.0), Triple(182.0, 93.0, 49.0), Triple(274.0, 31.0, 57.0)),
    OCEAN("Ocean", Triple(207.0, 50.0, 14.0), Triple(207.0, 30.0, 90.0), Triple(199.0, 89.0, 64.0), Triple(259.0, 50.0, 67.0)),
    NIGHT("Night", Triple(222.0, 47.0, 11.0), Triple(222.0, 65.0, 82.0), Triple(198.0, 93.0, 60.0), Triple(234.0, 89.0, 74.0)),
    DRACULA("Dracula", Triple(231.0, 15.0, 18.0), Triple(60.0, 30.0, 96.0), Triple(326.0, 100.0, 74.0), Triple(265.0, 89.0, 78.0)),
    SYNTHWAVE("Synthwave", Triple(254.0, 59.0, 26.0), Triple(260.0, 60.0, 98.0), Triple(321.0, 70.0, 69.0), Triple(197.0, 87.0, 65.0)),
    HALLOWEEN("Halloween", Triple(0.0, 0.0, 13.0), Triple(0.0, 0.0, 83.0), Triple(32.0, 89.0, 52.0), Triple(271.0, 46.0, 42.0)),
    COFFEE("Coffee", Triple(306.0, 19.0, 11.0), Triple(37.0, 30.0, 70.0), Triple(30.0, 67.0, 58.0), Triple(182.0, 25.0, 50.0)),
    BUSINESS("Business", Triple(0.0, 0.0, 13.0), Triple(0.0, 0.0, 82.0), Triple(210.0, 64.0, 55.0), Triple(200.0, 13.0, 65.0)),
    LUXURY("Luxury", Triple(240.0, 10.0, 4.0), Triple(37.0, 67.0, 58.0), Triple(0.0, 0.0, 100.0), Triple(218.0, 54.0, 50.0)),
    BLACK("Black", Triple(0.0, 0.0, 0.0), Triple(0.0, 0.0, 80.0), Triple(0.0, 0.0, 70.0), Triple(0.0, 0.0, 50.0)),
    CUPCAKE("Cupcake", Triple(24.0, 33.0, 97.0), Triple(280.0, 46.0, 14.0), Triple(183.0, 47.0, 59.0), Triple(338.0, 71.0, 78.0)),
    VALENTINE("Valentine", Triple(318.0, 46.0, 89.0), Triple(344.0, 38.0, 28.0), Triple(353.0, 74.0, 67.0), Triple(254.0, 86.0, 77.0)),
    PASTEL("Pastel", Triple(0.0, 0.0, 100.0), Triple(0.0, 0.0, 20.0), Triple(284.0, 22.0, 70.0), Triple(352.0, 70.0, 80.0)),
    FANTASY("Fantasy", Triple(0.0, 0.0, 100.0), Triple(215.0, 28.0, 17.0), Triple(296.0, 83.0, 35.0), Triple(200.0, 100.0, 37.0)),
    RETRO("Retro", Triple(45.0, 47.0, 80.0), Triple(345.0, 5.0, 15.0), Triple(3.0, 60.0, 55.0), Triple(145.0, 35.0, 50.0)),
    BUMBLEBEE("Bumblebee", Triple(0.0, 0.0, 100.0), Triple(0.0, 0.0, 20.0), Triple(41.0, 74.0, 53.0), Triple(50.0, 94.0, 58.0)),
    LEMONADE("Lemonade", Triple(0.0, 0.0, 100.0), Triple(0.0, 0.0, 20.0), Triple(89.0, 96.0, 31.0), Triple(60.0, 81.0, 45.0)),
    CORPORATE("Corporate", Triple(0.0, 0.0, 100.0), Triple(233.0, 27.0, 13.0), Triple(229.0, 96.0, 64.0), Triple(215.0, 26.0, 59.0)),
    CMYK("CMYK", Triple(0.0, 0.0, 100.0), Triple(0.0, 0.0, 20.0), Triple(203.0, 83.0, 60.0), Triple(335.0, 78.0, 60.0)),
    AUTUMN("Autumn", Triple(0.0, 0.0, 95.0), Triple(0.0, 0.0, 19.0), Triple(344.0, 96.0, 38.0), Triple(0.0, 63.0, 50.0)),
    WINTER("Winter", Triple(0.0, 0.0, 100.0), Triple(214.0, 30.0, 32.0), Triple(212.0, 100.0, 51.0), Triple(247.0, 47.0, 43.0)),
    ACID("Acid", Triple(0.0, 0.0, 98.0), Triple(0.0, 0.0, 20.0), Triple(303.0, 90.0, 45.0), Triple(27.0, 100.0, 50.0)),
    CYBERPUNK("Cyberpunk", Triple(56.0, 100.0, 50.0), Triple(56.0, 100.0, 10.0), Triple(345.0, 100.0, 50.0), Triple(195.0, 80.0, 55.0)),
    WIREFRAME("Wireframe", Triple(0.0, 0.0, 100.0), Triple(0.0, 0.0, 20.0), Triple(0.0, 0.0, 40.0), Triple(0.0, 0.0, 60.0)),
    LOFI("Lofi", Triple(0.0, 0.0, 100.0), Triple(0.0, 0.0, 0.0), Triple(0.0, 0.0, 5.0), Triple(0.0, 0.0, 30.0));

    val backgroundColor: Color get() = hsl(bg.first, bg.second, bg.third)
    val foregroundColor: Color get() = hsl(fg.first, fg.second, fg.third)
    val primaryColor: Color get() = hsl(primary.first, primary.second, primary.third)
    val accentColor: Color get() = hsl(accent.first, accent.second, accent.third)
    val isDark: Boolean get() = bg.third <= 50.0

    fun colorScheme(): ColorScheme {
        val onBg = foregroundColor
        val surface = if (isDark) hsl(bg.first, bg.second, (bg.third + 6).coerceAtMost(100.0))
        else hsl(bg.first, bg.second, (bg.third - 4).coerceAtLeast(0.0))
        val base = if (isDark) darkColorScheme() else lightColorScheme()
        return base.copy(
            primary = primaryColor,
            onPrimary = if (isDark) Color.Black else Color.White,
            secondary = accentColor,
            tertiary = accentColor,
            background = backgroundColor,
            onBackground = onBg,
            surface = surface,
            onSurface = onBg,
            surfaceVariant = surface,
            onSurfaceVariant = onBg.copy(alpha = 0.7f),
            outline = onBg.copy(alpha = 0.3f),
        )
    }

    companion object {
        fun fromName(name: String?): AppTheme =
            entries.firstOrNull { it.name == name } ?: NIGHT
    }
}
