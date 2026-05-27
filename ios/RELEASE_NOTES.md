## ATS-Mini Remote 3.1.0 — Theming + single-screen Radio

Builds on the 3.0.0 navigation refresh by porting the full HomeBoy theme system, consolidating the Radio tab onto a single screen with no scroll, and fixing the toolbar Bluetooth pill.

### Theming
- **30 color themes** ported from the Homebox / HomeBoy palette: Homebox, Light, Dark, Forest, Garden, Emerald, Aqua, Ocean, Night, Dracula, Synthwave, Halloween, Coffee, Business, Luxury, Black, Cupcake, Valentine, Pastel, Fantasy, Retro, Bumblebee, Lemonade, Corporate, CMYK, Autumn, Winter, Acid, Cyberpunk, Wireframe, Lo-Fi
- Solid backgrounds (no gradients), HSL-driven, persisted across launches
- Light / dark color scheme follows the active theme's background lightness automatically
- Theme picker in **Settings → Appearance** with circular swatches in a 4-column grid

### Radio screen — fits one screen, no scroll
- Removed the duplicate Band / Mode / Step / BW chip row from the Frequency card (the same info now appears in Controls right below)
- Controls replaces 5 stacked +/− rows with a wrap-flowing row of compact popup-menu pills (Band, Mode, Step, BW, AGC). Each pill shows the current value and tap exposes Next / Previous
- Signal card collapsed: RSSI / SNR / Battery as a single row of mini meters, CPU 0 / CPU 1 / Seq on a thin pill row underneath
- Inline navigation titles throughout — reclaims vertical space

### Toolbar Bluetooth pill
- The previous pill (tiny dot in a glass capsule) was hard to read. Replaced with a 34pt circular button using a recognizable BLE icon — slashed antenna when disconnected, dotted radiowaves when connected — theme-tinted, with a soft pulsing ring while connected.

### Settings
- New **Appearance** section above Firmware
- New **About** section with version, build, GitHub link
- Renamed `FirmwareUpdateView` → `SettingsView` for clarity

### Misc
- New `CLAUDE.md` at the repo root documenting workflow + design preferences for future automation

---

Unsigned IPA — sideload with AltStore / SideStore / Feather.
