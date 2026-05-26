## ATS-Mini Remote 3.0.0 — Major UI/UX refresh

A ground-up redesign of the iOS app navigation and interaction model, inspired by iOS 26 Human Interface Guidelines and HomeBoy-style minimal-tab design.

### Navigation
- Reduced from 5 tabs to 3: **Radio · Visualize · Settings**
- **Log** moved from a tab to a toolbar sheet (top-right of Radio tab) with a Clear action
- **Spectrum + Waterfall** merged into a single **Visualize** tab with a segmented picker

### Connection
- Connection status moved from a full card to a compact pill in the navigation bar — always visible, never eats scroll space
- Device picker sheet redesigned with Connected / Available sections and proper disconnect flow

### Radio screen
- RDS data (station, PTY, scroll text) now appears **inline** under the frequency display when on FM, instead of a separate card that pops in and out
- Improved seek / tune button feedback

### Signal & Status
- Removed redundant RSSI/SNR/Seq stat boxes
- CPU usage now shown as compact side-by-side gradient meters
- Battery icon dynamically reflects charge level (0% → 100%)

### Haptics throughout
- Medium impact: tune, scan, save, load, Sleep
- Light impact: delta +/− controls, zoom, volume change
- Heavy impact: Sleep action

### Empty states
- Spectrum / Waterfall canvases show a friendly "No data" message instead of an empty black box
- Log screen has a proper empty state with icon
- Presets card guides you to run a scan first

### Under the hood
- Compatible with iOS 26's native Liquid Glass throughout
- No new dependencies; pure SwiftUI

---

Unsigned IPA — sideload with AltStore / SideStore / Feather.
