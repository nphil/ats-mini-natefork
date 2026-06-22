# CLAUDE.md

## Workflow

- Push directly to `main`: `git push origin HEAD:main`
- CI conflict recovery: `git fetch origin main && git rebase origin/main && git push origin HEAD:main`
- Never push tags or bump versions manually — CI handles both.
- Before every commit: `grep VER_APP ats-mini/Common.h` → CHANGELOG entry must be labeled `VER_APP + 1`.
- **Every release needs a CHANGELOG entry** (feature or fix). No silent releases.

## Release Checklist (MANDATORY — applies to every session, every push)

Every push to `main` MUST result in a complete release. Before pushing, verify all of the following are in order:

1. **CHANGELOG entry exists** — `changelog/+<name>.<type>.md` fragment present for this change. Label = `VER_APP + 1`. CI assembles the entry; you must author the fragment.
2. **Firmware binaries** — CI publishes 4 assets automatically on push to `main`:
   - `*-ospi-ota.bin` (Web OTA)
   - `*-ospi-flash.bin` (preserves LittleFS)
   - `*-ospi-full.bin` (full erase)
   - `*-ospi-recovery.bin` (factory partition, flash to 0x10000)
   If firmware was not changed in this push, bins are still rebuilt (CI always runs on push).
3. **Android APK** — built and published as `android-v*` release; linked in firmware release notes. CI publishes automatically when `android/` changes. If Android changed, verify the new `android-v*` tag appears and is linked.
4. **iOS IPA** — built and published as `ios-v*` release; linked in firmware release notes. CI publishes automatically when `ios/` changes. If iOS changed, verify the new `ios-v*` tag appears and is linked.
5. **Release notes include all download links** — firmware release body must have the Downloads table (4 firmware bins + iOS IPA link + Android APK link). The CI `build.yml` Python step handles this with `per_page=100`.

**After every push, confirm the release is published** by checking the GitHub releases page (or using `mcp__github__list_releases`). If assets are missing, diagnose the CI job that failed rather than pushing a workaround.

**Do not skip the CHANGELOG fragment.** A push with no fragment produces a release with empty notes, which is not acceptable.

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

**Partition layout** (identical `partitions.csv` in both sketch dirs — verify against the file, sizes below are exact):

| Partition | Type | Address | Size | Role |
|-----------|------|---------|------|------|
| nvs | nvs | 0x9000 | 0x5000 | core NVS |
| otadata | ota | 0xe000 | 0x2000 | boot-slot selector |
| factory | app | 0x10000 | 0x180000 (1.5 MB) | **recovery** (never OTA'd) |
| ota_0 | app | 0x190000 | 0x2C0000 (2.75 MB) | **main app — the ONLY slot that persistently boots** |
| ota_1 | app | 0x450000 | 0x1B0000 (**1.6875 MB**) | **USB OTA staging only** (not a boot target) |
| littlefs | littlefs | 0x600000 | 0x1e0000 (1.875 MB) | user data |
| settings | nvs | 0x7e0000 | 0x10000 | shared NVS (namespace `recovery`, partition label `settings`) |
| coredump | coredump | 0x7f0000 | 0x10000 | crash dumps |

### CRITICAL: Boot architecture — why `ota_0` is the only persistent slot

This board does **not** use normal A/B OTA. The boot flow is deliberately recovery-first:

1. Main app **erases otadata on every boot** (`ats-mini.ino` `setup()`), so otadata = `0xFF`.
2. With otadata `0xFF`, the bootloader boots **factory (recovery)** first, every power-on.
3. Recovery's 2s splash, if not interrupted, calls `esp_ota_set_boot_partition(ota_0)` and reboots → boots **ota_0**.
4. Main app (ota_0) runs, erases otadata again → loop repeats next power cycle.

**Consequence (burned a lot of debugging time — do not forget):** an `esp_ota` "switch to the *next* slot"
update (writing ota_1 + `set_boot_partition(ota_1)`) **runs once then reverts** — the next power cycle
goes recovery → ota_0 (the old image). `ota_0` is the single canonical main slot; `ota_1` is scratch.
Any persistent main-firmware update **must land in ota_0**, and the only safe way to write ota_0 is
from recovery (you can't overwrite the slot you're executing from).

- Merged image omits `boot_app0.bin` at `0xe000` → otadata `0xFF` → bootloader boots factory.
- Boot-loop counter: `Preferences("recovery")` key `bootcount`; incremented before forwarding, reset in main `setup()`. At 3 → stays in recovery UI.
- Shared NVS handshake (main app ↔ recovery): both open `Preferences.begin("recovery", false, "settings")`. Keys: `bootcount`; `migPend/migSize/migCrc/migFail` (recovery self-migration → factory); `fwStagePend/fwStageSz/fwStageCrc/fwStageFail` (USB firmware staging → ota_0).

### USB firmware flashing (`Remote.cpp` serial OTA + recovery installer)

Two facts make this work; both were non-obvious:

1. **Flow control is mandatory.** The S3 native USB-Serial/JTAG (HWCDC) RX buffer is 256 B by default.
   Streaming a multi-MB image with no pacing overruns it → silently dropped bytes → transfer never
   completes ("No completion response"). Fix: `Serial.setRxBufferSize(8192)` before `Serial.begin()`,
   and a **per-block ACK** protocol — firmware acks every `SOTA_BLOCK` (4 KB) as `{"t":"ota","ack":N,"total":T}`;
   the app sends one block and waits for the ack before the next. (This is exactly why esptool works and
   a raw blast doesn't — esptool is request/response per block.)
2. **Install goes through recovery (mirrors the boot architecture above).** USB **Firmware** = stream raw
   into `ota_1` (staging, CRC-checked), record `fwStage*` in NVS, erase otadata, reboot → recovery's
   `installStagedFirmware()` copies `ota_1` → `ota_0` via the same `esp_ota` path the Wi-Fi OTA uses
   (validates SHA-256, sets boot, CRC read-back), then reboots into the new firmware. Power-loss safe
   (flag cleared only after ota_0 verifies; interrupted install re-runs from intact ota_1) and brick-safe
   (running slot untouched; bootloader falls back to factory if ota_0 is invalid). Needs **both** v2.72+
   main and recovery (the installer lives in recovery).
   - USB **Recovery** flashing (`rec_begin`) writes the factory partition **in place** (recovery image ≤ 1.5 MB).
   - Serial OTA protocol: `{"cmd":"ota_begin"|"rec_begin","size":N,"crc":C}` → `{"ok":true,...,"block":4096}`
     → blocks + acks → `{"fin":1}` (verifying heartbeat) → `{"ok":true,"staged":1}` (fw) or `{"ok":true}` (rec).
   - CRC32 = zlib/IEEE, sent from Java as a **signed** int32 (`CRC32().value.toInt()`) so the firmware's `atol` round-trips it.

**Staging size ceiling:** `ota_1` is **1.6875 MB**; the main app is ~1.6 MB (~90 KB headroom). When the app
outgrows `ota_1`, USB staging breaks — repartition (shrink factory/littlefs to grow ota_1) is a breaking,
full-reflash change. Wi-Fi recovery OTA writes ota_0 directly (2.75 MB) and is not subject to this limit.

**Extending recovery to alternative firmwares (future):** recovery is the universal installer — it's the only
context that can write `ota_0`. To boot an alternative firmware, have recovery install the chosen image to
`ota_0` (same `installStagedFirmware` mechanism). There is no spare *bootable* app slot (ota_1 is staging,
factory is recovery), so "multiple installed firmwares" means: keep the images elsewhere (littlefs / staged in
ota_1 / re-downloaded) and let recovery swap the selected one into `ota_0`. Any new app firmware must keep the
recovery-first contract: erase otadata in `setup()`, reset `bootcount`, and fit within `ota_0` (2.75 MB).

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
- Android USB flash has two methods (`ToolsScreen.kt`):
  - **Live (default)** — serial OTA to the running firmware (`SerialOta.kt`): Firmware → stage ota_1 → recovery installs ota_0; Recovery → factory in place. No buttons. Needs v2.72+ on **both** main and recovery.
  - **Bootloader (esptool)** — `EspFlasher.kt`: ROM protocol, no stub, 1 KB blocks, manual download-mode entry (hold BOOT, tap RESET, release BOOT). Works from any state incl. bricked. Use for Full / unbrick or pre-v2.72.
- Native USB caveat: device→host (console) and ROM mode (esptool) always work; host→running-firmware needs the flow control above. DTR/RTS auto-reset into download mode is unreliable through the Android CDC driver — prefer manual BOOT+RESET for the Bootloader method.

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
