#!/usr/bin/env python3
"""Generate ats-mini Themes.cpp from the iOS app's 30 HSL theme palette."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple

# 30 themes from ATSMini/Sources/Views/Components/Theme.swift (HSL h/s/l)
# (name, bg, fg, primary, accent)
THEMES: list[tuple[str, tuple, tuple, tuple, tuple]] = [
    ("Homebox",    (0,0,100),    (0,0,20),     (139,16,43),  (97,37,93)),
    ("Light",      (0,0,100),    (215,28,17),  (259,94,51),  (314,100,47)),
    ("Dark",       (0,0,11),     (0,0,90),     (259,94,70),  (314,100,70)),
    ("Forest",     (0,12,8),     (0,12,82),    (141,72,42),  (141,75,48)),
    ("Garden",     (0,4,91),     (0,3,6),      (139,16,43),  (97,37,93)),
    ("Emerald",    (0,0,100),    (219,20,25),  (141,50,60),  (219,96,60)),
    ("Aqua",       (219,53,43),  (218,100,89), (182,93,49),  (274,31,57)),
    ("Ocean",      (207,50,14),  (207,30,90),  (199,89,64),  (259,50,67)),
    ("Night",      (222,47,11),  (222,65,82),  (198,93,60),  (234,89,74)),
    ("Dracula",    (231,15,18),  (60,30,96),   (326,100,74), (265,89,78)),
    ("Synthwave",  (254,59,26),  (260,60,98),  (321,70,69),  (197,87,65)),
    ("Halloween",  (0,0,13),     (0,0,83),     (32,89,52),   (271,46,42)),
    ("Coffee",     (306,19,11),  (37,30,70),   (30,67,58),   (182,25,50)),
    ("Business",   (0,0,13),     (0,0,82),     (210,64,55),  (200,13,65)),
    ("Luxury",     (240,10,4),   (37,67,58),   (0,0,100),    (218,54,50)),
    ("Black",      (0,0,0),      (0,0,80),     (0,0,70),     (0,0,50)),
    ("Cupcake",    (24,33,97),   (280,46,14),  (183,47,59),  (338,71,78)),
    ("Valentine",  (318,46,89),  (344,38,28),  (353,74,67),  (254,86,77)),
    ("Pastel",     (0,0,100),    (0,0,20),     (284,22,70),  (352,70,80)),
    ("Fantasy",    (0,0,100),    (215,28,17),  (296,83,35),  (200,100,37)),
    ("Retro",      (45,47,80),   (345,5,15),   (3,60,55),    (145,35,50)),
    ("Bumblebee",  (0,0,100),    (0,0,20),     (41,74,53),   (50,94,58)),
    ("Lemonade",   (0,0,100),    (0,0,20),     (89,96,31),   (60,81,45)),
    ("Corporate",  (0,0,100),    (233,27,13),  (229,96,64),  (215,26,59)),
    ("CMYK",       (0,0,100),    (0,0,20),     (203,83,60),  (335,78,60)),
    ("Autumn",     (0,0,95),     (0,0,19),     (344,96,38),  (0,63,50)),
    ("Winter",     (0,0,100),    (214,30,32),  (212,100,51), (247,47,43)),
    ("Acid",       (0,0,98),     (0,0,20),     (303,90,45),  (27,100,50)),
    ("Cyberpunk",  (56,100,50),  (56,100,10),  (345,100,50), (195,80,55)),
    ("Wireframe",  (0,0,100),    (0,0,20),     (0,0,40),     (0,0,60)),
    ("Lofi",       (0,0,100),    (0,0,0),      (0,0,5),      (0,0,30)),
]


def hsl_to_rgb(h: float, s: float, l: float) -> Tuple[int, int, int]:
    """HSL (h: 0-360, s/l: 0-100) → 8-bit RGB tuple."""
    H = (h % 360) / 360
    S = s / 100
    L = l / 100
    if S == 0:
        v = int(round(L * 255))
        return v, v, v
    q = L * (1 + S) if L < 0.5 else L + S - L * S
    p = 2 * L - q

    def hue_to_rgb(t: float) -> float:
        t = t % 1.0
        if t < 1 / 6: return p + (q - p) * 6 * t
        if t < 1 / 2: return q
        if t < 2 / 3: return p + (q - p) * (2 / 3 - t) * 6
        return p

    r = hue_to_rgb(H + 1 / 3)
    g = hue_to_rgb(H)
    b = hue_to_rgb(H - 1 / 3)
    return int(round(r * 255)), int(round(g * 255)), int(round(b * 255))


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def hsl565(h, s, l) -> int:
    return rgb565(*hsl_to_rgb(h, s, l))


def adjust(hsl: tuple, *, dl: float = 0, ds: float = 0) -> tuple:
    """Return HSL shifted by dl (lightness delta) and ds (saturation delta)."""
    h, s, l = hsl
    return (h, max(0, min(100, s + ds)), max(0, min(100, l + dl)))


def mix(a: tuple, b: tuple, t: float) -> tuple:
    """Linear interpolate two HSL tuples."""
    return (
        a[0] * (1 - t) + b[0] * t,
        a[1] * (1 - t) + b[1] * t,
        a[2] * (1 - t) + b[2] * t,
    )


def is_dark(bg: tuple) -> bool:
    return bg[2] <= 50


def derive(name: str, bg: tuple, fg: tuple, primary: tuple, accent: tuple) -> dict:
    """Map 4 HSL primitives → 44 RGB565 fields for the ColorTheme struct."""
    dark = is_dark(bg)
    # Push muted text 35% toward bg (less contrast vs fg)
    muted = mix(fg, bg, 0.35)
    # Subtle panel-line color, ~8% delta from bg
    line = adjust(bg, dl=+8) if dark else adjust(bg, dl=-8)
    # Empty meter ~ 12% delta from bg
    empty = adjust(bg, dl=+12) if dark else adjust(bg, dl=-12)
    # Warm warning color tuned to mode; if accent is red-ish, reuse it.
    warn = (8, 88, 55)
    # Battery indicators
    good = (130, 70, 50)
    bad  = (5, 80, 55)
    # Highlight bg for menu selection — primary darkened/lightened toward bg
    hl_bg = mix(primary, bg, 0.55)
    # On/off box — accent for "on", warn for "off"
    box_off = (5, 75, 55)
    return {
        "bg":               hsl565(*bg),
        "text":             hsl565(*fg),
        "text_muted":       hsl565(*muted),
        "text_warn":        hsl565(*warn),

        "smeter_icon":      hsl565(*muted),
        "smeter_bar":       hsl565(*accent),
        "smeter_bar_plus":  hsl565(*primary),
        "smeter_bar_empty": hsl565(*empty),

        "save_icon":        hsl565(*primary),
        "stereo_icon":      hsl565(*muted),

        "rf_icon":          hsl565(*muted),
        "rf_icon_conn":     hsl565(*good),

        "batt_voltage":     hsl565(*fg),
        "batt_border":      hsl565(*muted),
        "batt_full":        hsl565(*good),
        "batt_low":         hsl565(*bad),
        "batt_charge":      hsl565(*bg),
        "batt_icon":        hsl565(*primary),

        "band_text":        hsl565(*muted),

        "mode_text":        hsl565(*muted),
        "mode_border":      hsl565(*muted),

        "box_bg":           hsl565(*bg),
        "box_border":       hsl565(*muted),
        "box_text":         hsl565(*muted),
        "box_off_bg":       hsl565(*box_off),
        "box_off_text":     hsl565(*fg),

        "menu_bg":          hsl565(*bg),
        "menu_border":      hsl565(*primary),
        "menu_hdr":         hsl565(*fg),
        "menu_item":        hsl565(*muted),
        "menu_hl_bg":       hsl565(*hl_bg),
        "menu_hl_text":     hsl565(*fg),
        "menu_param":       hsl565(*muted),

        "freq_text":        hsl565(*fg),
        "funit_text":       hsl565(*muted),
        "freq_hl":          hsl565(*accent),
        "freq_hl_sel":      hsl565(*primary),

        "rds_text":         hsl565(*muted),

        "scale_text":       hsl565(*fg),
        "scale_pointer":    hsl565(*accent),
        "scale_line":       hsl565(*line),

        "scan_grid":        hsl565(*line),
        "scan_snr":         hsl565(*primary),
        "scan_rssi":        hsl565(*accent),
    }


FIELD_ORDER = [
    "bg", "text", "text_muted", "text_warn",
    "smeter_icon", "smeter_bar", "smeter_bar_plus", "smeter_bar_empty",
    "save_icon", "stereo_icon",
    "rf_icon", "rf_icon_conn",
    "batt_voltage", "batt_border", "batt_full", "batt_low", "batt_charge", "batt_icon",
    "band_text",
    "mode_text", "mode_border",
    "box_bg", "box_border", "box_text", "box_off_bg", "box_off_text",
    "menu_bg", "menu_border", "menu_hdr", "menu_item", "menu_hl_bg", "menu_hl_text", "menu_param",
    "freq_text", "funit_text", "freq_hl", "freq_hl_sel",
    "rds_text",
    "scale_text", "scale_pointer", "scale_line",
    "scan_grid", "scan_snr", "scan_rssi",
]


def emit() -> str:
    out = ['#include "Common.h"',
           '#include "Themes.h"',
           '#include "Draw.h"',
           '#include <math.h>',
           '',
           '//',
           '// 30 themes inspired by the iOS app palette (Homebox / HomeBoy lineage).',
           '// Each iOS theme provides four HSL primitives (bg, fg, primary, accent);',
           '// the 44 RGB565 fields below are derived mechanically from those four',
           '// (see tools/gen_themes.py).',
           '//',
           'ColorTheme theme[] =',
           '{']
    for name, bg, fg, primary, accent in THEMES:
        colors = derive(name, bg, fg, primary, accent)
        out.append('  {')
        out.append(f'    "{name}",')
        for f in FIELD_ORDER:
            out.append(f'    0x{colors[f]:04X}, // {f}')
        out.append('  },')
    out += ['};',
            '',
            'uint8_t themeIdx = 0;',
            'int getTotalThemes() { return(ITEM_COUNT(theme)); }',
            '',
            '//',
            '// Turn theme editor on (1) or off (0), or get current status (2)',
            '//',
            'bool switchThemeEditor(int8_t state)',
            '{',
            '  static bool themeEditor = false;',
            '  themeEditor = state == 0 ? false : (state == 1 ? true : themeEditor);',
            '  return themeEditor;',
            '}',
            '']
    return '\n'.join(out)


if __name__ == "__main__":
    print(emit())
