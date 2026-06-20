# ATS-Mini Remote for Android

Material 3 / Material You companion app for the ATS-Mini ESP32-S3 receiver,
optimized for both phones and tablets.

## Highlights

- **Adaptive layout** — bottom navigation on phones, navigation rail/drawer and a
  two-pane (controls + spectrum) layout on tablets and foldables.
- **Theming** — Material You dynamic color (Android 12+) plus all 30 HomeBoy
  palettes ported from the iOS app for cross-device consistency.
- **BLE + USB** — connect over Bluetooth LE *or* wired USB-OTG.

## Android-only powers (vs iOS)

- **USB serial console** with live ESP32 boot/panic log capture — diagnose a
  bootloop directly from the phone.
- **One-tap USB firmware flashing** (ESP32-S3 ROM bootloader) — recover a bricked
  radio with no PC.
- **Auto-join the "ATS-Mini Recovery" Wi-Fi AP and upload OTA firmware** in one
  tap (with Shizuku).
- **Silent USB permission + radio auto-toggle** via Shizuku.
- Home-screen widget, Quick Settings tile, and a media-style notification.

Sideload and auto-update via **Obtainium** straight from GitHub Releases.
