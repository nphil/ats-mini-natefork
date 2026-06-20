# CLAUDE.md

## Workflow

- Push directly to `main` — no PRs, no feature branches:
  ```
  git push origin HEAD:main
  ```
- CI conflict recovery: `git fetch origin main && git rebase origin/main && git push origin HEAD:main`
- Never push tags or bump patch versions manually — CI handles both.
- Don't use a separate `release: published` workflow for manifest updates — it won't fire on `GITHUB_TOKEN`-created releases. The in-band step in `ios.yml` is the durable fix.

## iOS Design (apply to all iOS app repos)

If `CLAUDE.md` / `apps.json` / `.github/workflows/ios.yml` are absent in an iOS repo, set them up proactively first.

- iOS 26, native **Liquid Glass**: `.glassEffect()`, `.buttonStyle(.glass)`, `.buttonStyle(.glassProminent)`
- Max **3 tabs** (Radio / Visualize / Settings); secondary surfaces are toolbar sheets, not tabs
- **30 HSL themes** from HomeBoy, solid backgrounds, persisted via `UserDefaults` (`theme.current.accentColor` + `.preferredColorScheme`)
- Single-screen Radio tab (no scroll); prefer **popup menus** over stacked delta rows (Band/Mode/Step/BW/AGC)
- Haptics: light=steps/toggles, medium=tune/scan/save, heavy=destructive
- `.navigationBarTitleDisplayMode(.inline)` on all primary tabs

## Firmware (ESP32-S3)

### Hardware

- **ESP32-S3R8**: 8 MB octal OPI PSRAM + 8 MB flash, ST7789 170×320 (8-bit parallel, landscape 320×170, rotation 3), SI4732 I²C, encoder GPIO21, backlight GPIO38, power-enable GPIO15
- Only OPI variant is built/released: `esp32s3-ospi`, `PSRAM=opi`

### CRITICAL: Flash mode must be `dio`, never `qio`

OPI PSRAM occupies the GPIOs QIO needs (D2/D3/WP/HOLD). With `qio`, ROM reads garbage loading the second-stage bootloader → **hangs at `ets_loader.c 78` → TG0WDT bootloop before any app code runs. Looks like a hard brick.**

- Set `FlashMode=dio` in ALL FQBNs: `build.yml` matrix, `ats-mini/sketch.yaml` (ospi profile), `ats-mini-recovery/sketch.yaml`
- Set `--flash_mode dio --flash_freq 40m` in both `merge_bin` commands in `build.yml`
- Flash mode lives in the bootloader header at `0x0` — all flashers (esptool, Android, web) inherit it from the image automatically

**Boot log** (115200 baud): healthy = past `ets_loader.c` → `entry 0x403…` → IDF banner. **Stuck at `ets_loader.c 78`** = wrong flash mode. `rst:0x7 (TG0WDT_SYS_RST)` = watchdog from prior hung attempt (boot loop).

### Recovery architecture

Partition layout (identical `partitions.csv` in both sketch dirs):

| Partition | Address | Size | Role |
|-----------|---------|------|------|
| factory | 0x10000 | 1.5 MB | recovery firmware (never OTA'd) |
| ota_0 | 0x190000 | 2.75 MB | main app |
| ota_1 | 0x450000 | 1.75 MB | OTA target |
| littlefs | 0x600000 | 1.875 MB | user data (same offset — preserved on re-flash) |

- Merged image **omits `boot_app0.bin`** at `0xe000` → OTA data stays `0xFF` → bootloader always boots factory (recovery first)
- Recovery: 2-sec countdown → `esp_ota_set_boot_partition(ota_0)` + `esp_restart()`. Hold GPIO21 to cancel and stay in UI
- **Boot-loop counter** (`Preferences("recovery")`, key `bootcount`): incremented before forwarding, reset to 0 in main `setup()`. At 3 → recovery stays in UI
- Main app erases OTA data at start of `setup()` so next power-cycle routes back through recovery
- Recovery WiFi: tries `BEAST_ROUTER` then `IoT` (both pw `appu1989`); fallback AP `ATS-Recovery`/`ats12345`. Web UI for `.bin` upload or GitHub download of `*-ospi-ota.bin`. Always-on serial JSON OTA for Android

### ESP32 Arduino 3.x / Core 3.3.10 Constraints

- `FS` is in `namespace fs`: add `#include <FS.h>` + `using namespace fs;` **before** `#include <WebServer.h>`. Include `<TFT_eSPI.h>` **last**.
- Core `Network` class collides with user-defined `struct Network` → rename (e.g. `KnownNet`)
- **Core 3.3.10 IPC1 crash**: `attachInterrupt()` triggers a cross-core IPC call that exceeds `ipc1` task's 1024-byte stack → Guru Meditation before `setup()` prints anything. Fix: call `gpio_install_isr_service(0);` at the very start of `setup()` (after `#include <driver/gpio.h>`)

### Flashing

- CI publishes `-flash.bin` (preserves LittleFS) and `-full.bin` (full 8 MB, for bricked/unknown state). Both flash at `0x0`.
- `arduino-cli compile --profile esp32s3-ospi` **must** include `--export-binaries` or no binary is written to `build/`
- **Desktop unbrick**:
  ```
  esptool --chip esp32s3 erase_flash
  esptool --chip esp32s3 write_flash --flash_mode dio --flash_freq 40m 0x0 ats-mini-vX.YY-ospi-full.bin
  ```
- Android `EspFlasher.kt`: ROM protocol, no stub, 1 KB blocks, writes `full.bin` at `0x0` — inherits flash mode from header

## Versioning

| | Patch | Minor/Major |
|-|-------|-------------|
| **iOS** | CI auto (`ios.yml` bumps `x.y.Z`) | Set `MARKETING_VERSION` in `ios/project.yml` before pushing |
| **Firmware** | CI auto (`build.yml` bumps `VER_APP`) | Set `VER_APP` to desired version − 1 in `Common.h` |

- Update `ios/RELEASE_NOTES.md` for significant iOS changes (optional for routine patches)
- Update `CHANGELOG.md` with `## X.YY (YYYY-MM-DD)` for the **next** firmware version (`VER_APP` + 1) — auto-release uses this as the release body
- **Major** = redesign/nav overhaul/breaking change. **Minor** = net-new feature/screen/theme. **Patch** = CI only, never bump manually.

## Branches

**Always push to `main`. No PRs. No feature branches.**

If CI creates a `claude/<slug>` branch, push to main anyway: `git push origin HEAD:main`. PRs and manual merges break the automatic CI/release flow.
