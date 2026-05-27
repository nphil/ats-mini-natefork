## ATS-Mini Remote 3.0.0 — Major UI/UX refresh

A ground-up redesign of the iOS app navigation, controls, and theming, inspired by iOS 26 Human Interface Guidelines and HomeBoy-style minimal-tab design.

### Navigation
- Reduced from 5 tabs to 3: **Radio · Visualize · Settings**
- **Log** moved from a tab to a toolbar sheet (top-right of Radio tab) with a Clear action
- **Spectrum + Waterfall** merged into a single **Visualize** tab with a segmented picker
- Inline navigation titles throughout — more room for content, no scrolling on the Radio screen

### Theming
- **30 themes** ported from the Homebox / HomeBoy palette (Homebox, Dark, Forest, Garden, Emerald, Aqua, Ocean, Night, Dracula, Synthwave, Halloween, Coffee, Business, Luxury, Black, Cupcake, Valentine, Pastel, Fantasy, Retro, Bumblebee, Lemonade, Corporate, CMYK, Autumn, Winter, Acid, Cyberpunk, Wireframe, Lo-Fi, Light)
- Solid theme backgrounds (no gradients), HSL-driven, persisted across launches
- Light / dark color scheme follows the active theme's background lightness
- Theme picker in Settings → Appearance with circular swatches

### Connection
- New compact Bluetooth pill in the navigation bar with a recognizable BLE icon, color-coded by connection state and pulsing softly when connected
- Device picker sheet redesigned with Connected / Available sections

### Radio screen — single-screen, no-scroll
- Band / Mode / Step / BW / AGC consolidated into a row of compact popup-menu pills (current value visible, tap for Next / Previous)
- RDS data (station, PTY, scroll text) appears inline under the frequency display when on FM
- Compact Signal section: RSSI / SNR / Battery as a single row of mini meters, plus CPU and sequence number on a secondary row
- Volume slider with speaker icons, more compact spacing

### Haptics throughout
- Medium impact: tune, scan, save, load preset, Sleep
- Light impact: parameter menu steps, zoom, volume change
- Heavy impact: Sleep action

### Empty states
- Spectrum / Waterfall canvases show a friendly "No data" message instead of an empty black box
- Log screen has a proper empty state with icon
- Presets card guides you to run a scan first

### Settings
- New Settings tab with **Appearance** (theme picker), **Firmware** (OTA from file or URL), and **About** sections

### Under the hood
- Compatible with iOS 26's native Liquid Glass throughout
- No new dependencies; pure SwiftUI

---

Unsigned IPA — sideload with AltStore / SideStore / Feather.
