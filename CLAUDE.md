# CLAUDE.md

## Workflow

- Push directly to `main`: `git push origin HEAD:main`
- CI conflict recovery: `git fetch origin main && git rebase origin/main && git push origin HEAD:main`
- Never push tags or bump versions manually — CI handles both.
- Before every commit: `grep VER_APP ats-mini/Common.h` → CHANGELOG entry must be labeled `VER_APP + 1`.
- **Every release needs a CHANGELOG entry** (feature or fix). No silent releases.

## CI/CD

### Release assets (`firmware-auto-release` in `build.yml`)

Every push to `main` auto-bumps `VER_APP`, tags `vX.YY`, publishes release with:
- `*-ospi-ota.bin` — Web OTA via recovery UI
- `*-ospi-flash.bin` — `esptool write_flash 0x0` (preserves LittleFS)
- `*-ospi-full.bin` — `esptool write_flash 0x0` (erases everything)
- `*-ospi-recovery.bin` — `esptool write_flash 0x10000` (bootloader mode only)
- iOS IPA + Android APK linked from latest `ios-v*` / `android-v*` releases

Release notes Python step queries `per_page=100` for iOS/Android (must be 100+, not 20 — there are 30+ firmware releases that push ios/android past page 1).

### Pre-commit rules

- No trailing whitespace on ANY line (including blank lines) in ANY file. Hooks fail hard.
- LF line endings only. Strip with: `sed -i 's/[[:space:]]*$//' <files>`

### Removed — do not re-add

- `latest-release` job — ruleset blocks `latest` tag (HTTP 422). Use `--make-latest=true` on `gh release create`.
- `Attach latest iOS IPA` step — IPA linked by URL in notes, not re-uploaded.

## iOS Design

- iOS 26, **Liquid Glass**: `.glassEffect()`, `.buttonStyle(.glass)`, `.buttonStyle(.glassProminent)`
- Max **3 tabs** (Radio / Visualize / Settings); secondary surfaces are toolbar sheets
- **30 HSL themes**, solid backgrounds, `UserDefaults` (`theme.current.accentColor` + `.preferredColorScheme`)
- Single-screen Radio tab (no scroll); popup menus over stacked rows (Band/Mode/Step/BW/AGC)
- Haptics: light=steps/toggles, medium=tune/scan/save, heavy=destructive
- `.navigationBarTitleDisplayMode(.inline)` on all primary tabs

## Firmware (ESP32-S3)

### Hardware

- **ESP32-S3R8**: 8 MB OPI PSRAM + 8 MB flash, ST7789 170×320 (landscape, rotation 3), SI4732 I²C
- Encoder: GPIO2(A) GPIO1(B) GPIO21(btn) | Backlight: GPIO38 | Power-enable: GPIO15
- Only OPI variant built: `esp32s3-ospi`, `PSRAM=opi`

### CRITICAL: Flash mode = `dio`, never `qio`

OPI PSRAM uses the GPIOs QIO needs → garbage bootloader read → **hangs `ets_loader.c 78` → TG0WDT bootloop**.
- `FlashMode=dio` in ALL FQBNs: `build.yml`, `ats-mini/sketch.yaml`, `ats-mini-recovery/sketch.yaml`
- `--flash_mode dio --flash_freq 40m` in both `merge_bin` commands
- Boot log: healthy = past `ets_loader.c` → `entry 0x403…`. Stuck at line 78 = wrong flash mode.

### Recovery firmware (`ats-mini-recovery/`)

**Entry:** Press encoder during 2-second splash countdown, or auto-enter on boot-loop (3 fails).

**Partition layout** (identical `partitions.csv` in both sketch dirs):

| Partition | Address | Size | Role |
|-----------|---------|------|------|
| factory | 0x10000 | 1.5 MB | recovery (never OTA'd) |
| ota_0 | 0x190000 | 2.75 MB | main app |
| ota_1 | 0x450000 | 1.75 MB | OTA target |
| littlefs | 0x600000 | 1.875 MB | user data |

- Merged image omits `boot_app0.bin` at `0xe000` → OTA data = `0xFF` → bootloader always boots factory
- Boot-loop counter: `Preferences("recovery")` key `bootcount`; incremented before forwarding, reset in main `setup()`. At 3 → stays in recovery UI.
- Main app erases OTA data in `setup()` so next power-cycle re-enters recovery.

**WiFi:** No AP on boot. Async STA tries `BEAST_ROUTER` then `IoT` (both pw `appu1989`). Toast in footer shows status. User starts open adhoc AP (`ATS-Recovery`, no password) from Network menu.

**Network menu:** WiFi Scan | OTA Update | Adhoc Network | Web Server | Back

**OTA:** Requires STA connected (not adhoc) + TCP ping to 8.8.8.8:53 before fetching. Retries 3×. Download loop driven by byte count (not `http.connected()`) to avoid 99% stall.

**Navigation:** Encoder scrolls menus. Short press = select. 450ms hold = screen-specific action. 2-second hold = global escape to main menu from any screen.

**Keyboard:** Encoder walks all keys linearly across all 4 rows. Short press = select key. No long-press-for-row (conflicts with 2s escape).

**Flicker-free rendering:** `fullDraw` flag controls full vs partial redraws. Only changed rows/keys repaint on encoder steps. Footer shows toast (yellow, priority) or network status.

**Flash recovery to device:** `python -m esptool --chip esp32s3 --port COMXX --baud 921600 write-flash --flash-mode dio --flash-freq 40m 0x10000 ats-mini-recovery.ino.bin`

### BLE (`ats-mini/Ble.h` + `Remote.cpp`)

- **Nordic UART** service/characteristics. TX = notify, RX = write.
- `write()` uses `vTaskDelay(12ms)` between chunks (NOT `delay()` — busy-wait froze main loop).
- On connect: requests 20–40ms connection interval via `updateConnParams()`.
- Status sends are **change-driven**: only fires when freq/mode/band/BFO/vol/AGC/RSSI/SNR changes (floor = subscription interval). 2s keepalive when idle.
- Subscription interval: 250ms (floor, not constant rate). Apps send `{"cmd":"sub","ms":250}` on connect.
- MTU: Android requests 517, iOS uses default (~186). Chunk size = `getMTU() - 3`.

### ESP32 Arduino 3.x / Core 3.3.10

- `#include <FS.h>` + `using namespace fs;` **before** `#include <WebServer.h>`. `<TFT_eSPI.h>` **last**.
- `struct Network` collides with Core `Network` class → rename (e.g. `KnownNet`).
- IPC1 crash: call `gpio_install_isr_service(0)` at very start of `setup()` before any `attachInterrupt()`.

### Flashing

- `arduino-cli compile --profile esp32s3-ospi --export-binaries`
- Desktop unbrick: `esptool --chip esp32s3 erase_flash` then `write_flash --flash_mode dio --flash_freq 40m 0x0 *-full.bin`
- Android `EspFlasher.kt`: ROM protocol, no stub, 1 KB blocks, `full.bin` at `0x0`.

## Versioning

| | Patch | Minor/Major |
|-|-------|-------------|
| **iOS** | CI auto | Set `MARKETING_VERSION` in `ios/project.yml` |
| **Firmware** | CI auto | Set `VER_APP` to target − 1 in `Common.h` |

- CHANGELOG entry label = `VER_APP + 1` (the version CI will tag, not the current value).
- Update `ios/RELEASE_NOTES.md` for significant iOS changes.
- **Major** = redesign/breaking. **Minor** = new feature/screen. **Patch** = CI only.

## Android App

- `NavigationSuiteScaffold` for adaptive nav (phone/tablet).
- `enableEdgeToEdge()` in MainActivity + `statusBarsPadding()` on content `Box` — no status bar overlap.
- No page-level scrolling on any tab. Internal scroll only where content genuinely overflows.
- Tools tab: segmented **USB flash / Wi-Fi OTA / Console**. Progress state hoisted above panel switch.
- Recovery OTA: `RECOVERY_SSID = "ATS-Recovery"`, `RECOVERY_PASS = ""` (open adhoc, no password).
- BLE: `requestMtu(517)`, `SCAN_MODE_LOW_LATENCY`. Callbacks on Binder thread; StateFlow updates are thread-safe.

## Branches

**Always push to `main`. No PRs. No feature branches.**
