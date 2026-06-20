# CLAUDE.md

Project guidelines for Claude when working on this repo.

## Apply to all iOS apps

The conventions below are **the user's standard iOS-app deployment
template** — apply them to any iOS app repo, not just this one. If
working on a new iOS app and `CLAUDE.md` / `apps.json` /
`.github/workflows/ios.yml` aren't present in the template form below,
set them up proactively before shipping.

## Workflow preferences

- **Push directly to `main`** — no separate dev branch needed:
  ```
  git push origin HEAD:main
  ```
  CI conflict recovery (CI auto-bumps version after every push):
  ```
  git fetch origin main && git rebase origin/main && git push origin HEAD:main
  ```
- **Releases are fully automatic.** Every push to `main` triggers
  `ios.yml`, which auto-increments the patch version in `ios/project.yml`,
  builds the unsigned IPA, patches `apps.json`, commits the bump with
  `[skip ci]`, creates the `ios-vX.Y.Z` tag, and publishes the release.
- **Do not use a separate `release: published` workflow** for the manifest
  update — it won't fire when the release is created by `GITHUB_TOKEN`.
  The in-band step in `ios.yml` is the durable fix.
- Don't push tags by hand. The pipeline creates them.

## Design preferences (iOS app)

- iOS 26 target with native **Liquid Glass** throughout (`.glassEffect()`,
  `.buttonStyle(.glass)`, `.buttonStyle(.glassProminent)`).
- **3 tabs maximum** (currently Radio / Visualize / Settings); secondary
  surfaces like Log are toolbar sheets, not tabs.
- **30 HSL color themes** ported from HomeBoy, solid backgrounds only (no
  gradients). Theme is persisted to UserDefaults and applied via
  `theme.current.accentColor` + `.preferredColorScheme`.
- **Single-screen primary view** — the Radio tab should fit on an iPhone
  screen without scrolling under normal conditions.
- Prefer **popup menus** over stacked +/- delta rows when surfacing
  several related controls (Band/Mode/Step/BW/AGC live in compact
  menu pills, not 5 stacked rows).
- **Haptics on every interactive control**: light for steps and toggles,
  medium for primary actions (tune, scan, save), heavy for destructive /
  significant actions (Sleep).
- **Inline navigation titles** in primary tabs (`.navigationBarTitleDisplayMode(.inline)`)
  to reclaim vertical space.

## Firmware (ESP32-S3 — this repo)

This repo also ships ESP32-S3 firmware (`ats-mini/`) plus an Android
companion app (`android/`). The findings below are hard-won — read them
before touching the firmware build, flashing, or boot path.

### Hardware

- **Board: ESP32-S3R8** — 8 MB **octal (OPI) PSRAM** + 8 MB flash, ST7789
  170×320 display driven over an **8-bit parallel** bus (landscape =
  320×170, rotation 3), SI4732 receiver on I²C, encoder push-button on
  **GPIO21**, LCD backlight on GPIO38, power-enable on GPIO15.
- The QSPI/quad-PSRAM variant exists (`esp32s3-qspi` profile,
  `PSRAM=enabled`) but is **not** built/released by CI — the shipped board
  is the OPI variant (`esp32s3-ospi`, `PSRAM=opi`).

### CRITICAL: flash mode MUST be `dio`, never `qio`

On octal-PSRAM boards (`PSRAM=opi`) the OPI PSRAM physically occupies the
GPIOs that QIO flash needs for its D2/D3/WP/HOLD data lines. The flash can
therefore only run with **two data lines = DIO mode**. A `qio` bootloader
header makes the ROM clock four data lines that are wired to the PSRAM, so
it reads garbage while loading the second-stage bootloader and **hangs at
`ets_loader.c 78`, then TG0WDT-resets in a loop — before any app, recovery,
or partition-table code runs.** This looks exactly like a hard brick.

- Set `FlashMode=dio` in **every** FQBN: `.github/workflows/build.yml`
  matrix, `ats-mini/sketch.yaml` (ospi profile), and
  `ats-mini-recovery/sketch.yaml`.
- Set `--flash_mode dio --flash_freq 40m` in **both** `merge_bin` commands
  in `build.yml` (the `-flash.bin` and `-full.bin` images).
- Flash mode lives in the **bootloader header at offset `0x0`**, so every
  flashing path (desktop esptool, Android `EspFlasher.kt`, web esptool-js)
  inherits it automatically from the released image — no per-flasher logic.

**Reading the boot log** (115200 baud): a healthy boot continues past
`ets_loader.c` to an `entry 0x403…` line and the IDF bootloader banner
(`I (xx) boot: …`). If output **stops at `ets_loader.c 78`** with no banner,
the flash mode is wrong (or the bootloader bytes are corrupt). `rst:0x7
(TG0WDT_SYS_RST)` in the header is the watchdog resetting a prior hung
attempt — i.e. a boot loop.

### Recovery architecture (two firmwares, recovery-first)

- Two sketches: `ats-mini/` (main app, runs from **ota_0**) and
  `ats-mini-recovery/` (minimal recovery, runs from the **factory**
  partition). `partitions.csv` is identical in both dirs:
  `factory@0x10000` (1.5 MB), `ota_0@0x190000` (2.75 MB),
  `ota_1@0x450000`, `littlefs@0x600000` (**unchanged offset — preserves
  user data on re-flash**), `settings` nvs, `coredump`.
- **Recovery always boots first.** The merged image intentionally **omits
  `boot_app0.bin`** at `0xe000`, so OTA data stays `0xFF` and the
  bootloader always falls back to the factory (recovery) partition.
- Recovery shows a **2-second countdown** then forwards to ota_0 via
  `esp_ota_set_boot_partition(ota_0)` + `esp_restart()`. Holding the
  encoder button (GPIO21) cancels the countdown and stays in the recovery
  UI.
- **Boot-loop counter** lives in NVS (`Preferences("recovery", …,
  "settings")`, key `bootcount`): recovery increments it before
  forwarding; the main app resets it to `0` at the end of `setup()`. If it
  reaches **3**, recovery stays in its UI instead of forwarding.
- The main app **erases OTA data at the very start of `setup()`** so every
  power-cycle routes back through recovery on the next boot.
- Recovery features: WiFi (tries `BEAST_ROUTER` then `IoT`, both pass
  `appu1989`; falls back to AP `ATS-Recovery` / `ats12345`), a web page to
  upload a `.bin` or trigger a GitHub download of the latest
  `*-ospi-ota.bin`, and an always-on serial JSON OTA protocol for the
  Android app.

### ESP32 Arduino 3.x / ESP32-S3 Core 3.3.10 Constraints

- `FS` moved into `namespace fs`. Put `#include <FS.h>` + `using namespace
  fs;` **before** `#include <WebServer.h>` or you get `'FS' was not
  declared in this scope`. Include `<TFT_eSPI.h>` **last**.
- A new core `Network` class collides with any user-defined `struct
  Network` — name such structs something else (e.g. `KnownNet`).
- **Core 3.3.10 Boot crash (IPC1 Stack Canary watchpoint)**: Standard Arduino
  `attachInterrupt()` triggers a cross-core IPC call to install the GPIO ISR service.
  Under Core 3.3.10, the precompiled stack size of the `ipc1` inter-core task is `1024` bytes.
  Memory allocations in this call exceed 1024 bytes and immediately trigger a stack
  canary Guru Meditation crash before `setup()`'s serial prints are reached.
  *Durable Workaround*: Always call `gpio_install_isr_service(0);` at the very beginning
  of `setup()` in the main app after including `<driver/gpio.h>` to allocate the interrupt
  service locally on Core 1 (8KB stack).

### Flashing & unbricking

- CI publishes `-flash.bin` (defined regions only — preserves LittleFS) and
  `-full.bin` (8 MB, every byte filled — for bricked/unknown state). Both
  flash at `0x0`.
- **Compilation Output**: When using `arduino-cli compile --profile esp32s3-ospi`, you
  **MUST** specify the `--export-binaries` flag. Without it, the binary is not written to
  the local `build/` folders, and flashing commands will upload stale files.
- **Desktop unbrick** (the definitive recovery, verified working):
  ```
  esptool --chip esp32s3 erase_flash
  esptool --chip esp32s3 write_flash --flash_mode dio --flash_freq 40m \
      0x0 ats-mini-vX.YY-ospi-full.bin
  ```
- The Android USB flasher (`android/.../usb/EspFlasher.kt`) uses the bare
  ROM protocol (no stub, 1 KB blocks) and writes `full.bin` at `0x0` — same
  operation as desktop esptool, just slower. It inherits the correct flash
  mode from the image header, so it needs no mode-specific code.

## Versioning

- **Patch bumps are handled by CI automatically** — `ios.yml` increments
  `x.y.Z` and `CURRENT_PROJECT_VERSION` on every main push. Do not bump
  the patch version manually.
- For **minor or major bumps** (new feature, redesign), set
  `MARKETING_VERSION` in `ios/project.yml` to the desired `x.Y.0` or
  `X.0.0` before pushing. CI will then patch-increment from there on the
  next release.
- Update `ios/RELEASE_NOTES.md` alongside significant changes — it becomes
  the GitHub Release body and Feather `localizedDescription`. For routine
  patch releases it is optional (CI uses it if present).
- For **firmware**, patch bumps are handled by CI automatically —
  `build.yml` increments `VER_APP` in `ats-mini/Common.h` on every push
  to `main` and publishes a `vX.YY` release with the OSPI flash binary
  attached. Do not bump `VER_APP` manually for routine changes.
- Update `CHANGELOG.md` with a `## X.YY (YYYY-MM-DD)` section *matching
  the next auto-bumped version* (current on-disk `VER_APP` + 1) — the
  auto-release uses that section as the release body. If no section
  exists, a generic note is published.
- For **minor or major firmware bumps**, set `VER_APP` directly in
  `Common.h` to one less than the desired version (e.g. set to `299`
  if you want the next release to be `v3.00`); CI auto-bumps on the
  next push.

### Version classification

- **Major (X.0.0)** — redesign, navigation overhaul, breaking change.
- **Minor (x.Y.0)** — net-new feature, new screen, new theming system.
- **Patch (x.y.Z)** — CI handles this automatically; no manual bump needed.

## Branches

- **ALWAYS push directly to `main`. No pull requests. No feature branches. No merging.**
- Never leave commits on a session branch — always land on `main` immediately:
  ```
  git fetch origin main && git rebase origin/main && git push origin HEAD:main
  ```
- If the session environment creates a dev branch (e.g. `claude/<slug>`), push
  to `main` anyway using `HEAD:main`. Do not wait for a PR or human merge step.
- The pipeline is fully automatic — every push to `main` triggers CI, bumps
  versions, and publishes releases. PRs and manual merges break this flow.
