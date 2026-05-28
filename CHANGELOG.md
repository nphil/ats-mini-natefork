# Changelog

The user manual is available at <https://esp32-si4732.github.io/ats-mini/manual.html>. The firmware flashing instructions are available at <https://esp32-si4732.github.io/ats-mini/flash.html>

<!-- towncrier release notes start -->

## 2.44 (2026-05-28)

### Fixed

- **BLE semaphore no longer constructed at global init time.** `std::binary_semaphore` inside the global `NordicUART BLESerial` object was being initialised before the FreeRTOS scheduler started (C++ global constructors run before `app_main()`). On this toolchain the semaphore's constructor internally calls into the FreeRTOS/pthreads layer, which crashes before the scheduler is up. Replaced with a `SemaphoreHandle_t` that is created lazily inside `BLESerial.start()` — the first point at which FreeRTOS is guaranteed to be running. This should fix the persistent bootloop on all hardware.

## 2.43 (2026-05-27)

### Fixed

- **Firmware version now shown immediately at boot.** The display and backlight come on in the first 100 ms of startup — before any WiFi, BLE, or SI4732 initialisation. This makes it easy to confirm which firmware is running, and means the screen is lit even if a later init step fails. Previously the backlight stayed dark until well into setup(), making any crash before that point look identical to a blank power-on.
- **Recovery mode now works without display re-init.** `checkRecoveryBoot()` is called after the display is already up, so the recovery screen appears immediately when the encoder button is held at power-on.

## 2.42 (2026-05-27)

### Fixed

- **Reverted BLE stack from NimBLE-Arduino back to Bluedroid.** NimBLE-Arduino performs global-level C++ constructor initialisation before `setup()` runs. On this hardware that crashes the device before any app code executes, producing a persistent bootloop where the display never comes on and USB keeps reconnecting. Restoring the original Bluedroid stack (used through v2.36) eliminates the crash. The Nordic UART service UUIDs and iOS app behaviour are unchanged.

  **If your device is stuck bootlooping:** download `ats-mini-v2.43-ospi-flash.bin` (or newer) from the Releases page and flash it with esptool at address `0x0`:
  ```
  esptool.py --chip esp32s3 write_flash 0x0 ats-mini-v2.43-ospi-flash.bin
  ```
  Use the `*-ospi-flash.bin` file (the merged binary), not the `*-ospi-ota.bin` file (which is only for web OTA uploads).

## 2.40 (2026-05-27)

### Added

- **Recovery mode.** Hold the encoder button while powering on (keep it held for 1 second). The device skips normal boot and enters a self-contained recovery UI: it creates a WiFi access point named `ATS-Mini Recovery` (password `atsrecover`), and once you connect a phone or PC you can visit `http://192.168.4.1` to upload a fresh `*-ospi-ota.bin` firmware file via a browser — no drivers, no esptool, no USB cable required. The recovery code cannot overwrite itself (OTA always targets the inactive slot).
- **OTA rollback guard.** The firmware now calls `esp_ota_mark_app_valid_cancel_rollback()` after all hardware (display, SI4732, WiFi, BLE) initialises successfully. If the app crashes before that point on a future boot, the ESP-IDF bootloader automatically rolls back to the previous working firmware on the next restart.

## 2.38 (2026-05-27)

### Changed

- **Redesigned WiFi menu with on-device network picker and keyboard.** The old web-form-only workflow is replaced by a three-screen flow directly on the radio: pick *Off / Access Point / Connect*, scan and choose a network from the list (with RSSI bars and lock icon for secured networks), then type the password on a full-screen on-device keyboard. Open networks skip the password step entirely.
- WiFi settings now offer only the three modes that matter: **Off**, **Access Point**, and **Connect** (join your home network). The unused AP+Connect and Sync-only modes have been removed; devices upgrading with those modes saved will automatically switch to Connect.

## 2.37 (2026-05-27)

### Changed

- **BLE now stays alive in CPU Sleep mode.** Previously, selecting `Settings → Sleep Mode → CPU Sleep` shut down the BLE stack and dropped any connected iOS app. The new implementation uses the ESP-IDF power-management framework (`esp_pm_configure()` with `light_sleep_enable = true`), which lets the FreeRTOS idle hook dip the CPU into light sleep between BLE/timer events automatically. An already-connected iOS app stays connected throughout sleep, and the device remains discoverable for new connections.
- **Migrated BLE stack from legacy Bluedroid to NimBLE-Arduino 2.x.** Roughly halves the BLE memory footprint (~30 KB freed from internal SRAM) and uses less CPU during connection events. Behaviour and Nordic UART service UUIDs are unchanged — the iOS app needs no update.
- CPU Sleep now respects the user-selected `Settings → CPU Freq` ceiling: the PM framework uses the chosen freq (80/160/240 MHz) as the max and 40 MHz as the min. Picking 240 MHz no longer disables effective sleep — the CPU still scales down to 40 MHz when idle.

### Removed

- The blocking `esp_light_sleep_start()` loop in `sleepOn()`. Replaced with PM-based automatic light sleep that wakes for BLE events.

## 2.36 (2026-05-27)

Best paired with **ATS-Mini Remote iOS app v3.2.x** which now reads the radio's IP/AP/version over BLE and offers one-tap WiFi OTA flashing from the GitHub releases list.

### Added

- BLE status JSON now exposes three OTA-helper fields: `wip` (the device's HTTP OTA IP — STA address when joined to your home Wi-Fi, AP address when in access-point mode), `wm` (WiFi mode: -1/0 off, 1 AP, 2 STA), and `fw` (the running firmware version as an integer, e.g. 236 for v2.36). Used by the iOS app to auto-populate the host field, mark the installed release with an `INSTALLED` badge, and disable the Flash button when the radio isn't reachable.
- New `getOTAIPAddress()` helper in `Network.cpp` that returns the best IP to POST firmware to — STA if connected, AP IP otherwise.

### Changed

- CI: OSPI-only builds. The QSPI matrix entry was dropped from `build.yml` since the OSPI variant covers every supported ATS-Mini unit. Build time halved, release artifact list trimmed.
- CI: every push to `main` now auto-bumps `VER_APP` and publishes a `vX.YY` release with the OSPI flash binary attached — no more manual `VER_APP` edits required. Mirrors how `ios.yml` auto-releases `ios-vX.Y.Z`.

## 2.35 (2026-05-27)

### Changed

- Bump settings version (VER_SETTINGS 73→74). The v2.34 theme palette was completely replaced (19 themes → 30 new themes), so any saved theme index now points to a different theme. On first boot after upgrading from v2.34 or earlier the settings are reset to defaults, letting the user pick from the new palette.

### Fixed

- Add bounds check on the saved theme index when loading settings. Prevents a potential out-of-bounds access if NVS holds an index from a build with a different theme count.
- Skip BLE initialisation at boot if free internal heap is below 80 KB. Prevents a hard crash on devices where the sprite buffer fell back to internal RAM (PSRAM unavailable), consuming heap the BLE stack also needs. Prints free-heap size to the serial console before BLE init to aid diagnosis.

## 2.34 (2026-05-27)

Best paired with **ATS-Mini Remote iOS app v3.2.0** which adds silent auto-reconnect — the app now remembers and quietly reconnects to the radio after sleep, reboot, or the 5-minute BLE auto-off cycle.

### Added

- BLE now comes up automatically at every boot regardless of the last saved state. After 5 minutes with no client connection it switches off to save power, but the OFF state is never persisted — the next power-up always brings BLE back up.
- Ship 30 new themes derived from the iOS app's HSL palette (Homebox, Light, Dark, Forest, Garden, Emerald, Aqua, Ocean, Night, Dracula, Synthwave, Halloween, Coffee, Business, Luxury, Black, Cupcake, Valentine, Pastel, Fantasy, Retro, Bumblebee, Lemonade, Corporate, CMYK, Autumn, Winter, Acid, Cyberpunk, Wireframe, Lofi) so the device and the iOS app look visually consistent.


### Fixed

- Fix crash when the device enters light sleep while a BLE client is connected. The teardown sequence now sets `started = false` before tearing down GATT state, gracefully disconnects active clients with a short propagation delay, and guards `onDisconnect` against post-stop invocation.


## 2.33 (2025-09-22)


### Changed

- Adjust gamma for display ID 0x04858552 so the themes look closer to how they were designed (at least Orange now doesn't look like a lemon).

## 2.32 (2025-09-16)


### Removed

- Remove the dynamic CPU frequecy feature introduced in v2.31 (it caused sound artifacts when rotating the encoder). [#244](https://github.com/esp32-si4732/ats-mini/issues/244)
- Do not show the "Add" hint on an empty memory slot to prevent confusion with click vs short press.


### Changed

- Avoid drawing background color when drawing text. This dramatically helps UI customization modding efforts (like setting a background image instead of a plain color, [for example](https://github.com/esp32-si4732/ats-mini/discussions/240)). [#239](https://github.com/esp32-si4732/ats-mini/issues/239)
- Move the Web UI credentials form fields below the Wi-Fi settings. [#241](https://github.com/esp32-si4732/ats-mini/issues/241)


### Fixed

- Fix Wi-Fi connection issue to 2nd or 3rd access point configured on the settings web page. [#244](https://github.com/esp32-si4732/ats-mini/issues/244)

## 2.31 (2025-09-13)


### Removed

- Remove faster tuning in Seek mode on SSB and in Scan mode via press & rotate in favor of the new accelerated encoder control.
- Remove the ENABLE_HOLDOFF compile-time option.


### Added

- Encoder acceleration.
- Encoder click now cancels the EiBi schedule download process.


### Changed

- Reduce the upper CB band limit to 28MHz. [#205](https://github.com/esp32-si4732/ats-mini/issues/205)
- Independent USB/LSB calibration values. WARNING: this change will reset the bands settings. [#220](https://github.com/esp32-si4732/ats-mini/issues/220)
- Render partial frequency numbers on the tuning scale around screen edges. [#235](https://github.com/esp32-si4732/ats-mini/issues/235)
- Disable the Memory menu timeout (it is a surfing mode like Seek or Scan). Short press (0.5 sec) saves/clears a slot, click closes the menu.
- EXPERIMENTAL: overclock the I2C bus to 800kHz (affects Si4732).
- Set CPU freq to 240 MHz on encoder rotation, drop back to 80 MHz after 10 seconds of no activity. This results in snappier UI.


### Fixed

- Fix AVC wrapping to avoid selecting odd AVC values. [#207](https://github.com/esp32-si4732/ats-mini/issues/207)
- Add 100ms delay after Si4732 POWER_ON to fix the "Si4732 not detected" issue [#213](https://github.com/esp32-si4732/ats-mini/issues/213)
- Fix misbehaving squelch when changing bands.

## 2.30 (2025-08-07)


### Added

- Add Scan mode. Press the encoder for 0.5 seconds to rescan, press & rotate to tune using a larger step. The scan process can be aborted by clicking or rotating the encoder.


### Changed

- Switch from EEPROM to Preferences library to store the receiver settings. This change removes some old limitations and enables more flexible settings management. WARNING: upgrading to this firmware version from an older one will reset the settings. Also a forced reset might be required (hold the encoder and power on the receiver). [#94](https://github.com/esp32-si4732/ats-mini/issues/94)
- Mute audio amp during seek action to prevent audible artifacts. [#190](https://github.com/esp32-si4732/ats-mini/issues/190)
- Display "Loading SSB" message in the zoomed menu area.
- Extend the 16m broadcast band a bit to include CRI on 17490.
- Increase the number of memory slots to 99.


### Fixed

- Do not lose SSB sub kHz digits when storing Memory slots. [#109](https://github.com/esp32-si4732/ats-mini/issues/109)
- Restore saved bandwidth.
- Use default step when switching modes or memories.

## 2.28 (2025-07-01)


### Added

- Add UTC+5:30 offset for India (Asia/Kolkata). Users with offsets greater than 5:30 might need to readjust their timezone settings (menu indexes have been shifted).
- Enable PSRAM using different build artifacts for OSPI and QSPI ESP32-S3 modules. For more info see <https://esp32-si4732.github.io/ats-mini/flash.html#firmware-files>.


### Changed

- Much better seek sensitivity (SI4735 library patch by @zhang-chong). [#129](https://github.com/esp32-si4732/ats-mini/issues/129)
- 200kHz FM step now uses odd frequencies (99.1, 99.3, etc). [#161](https://github.com/esp32-si4732/ats-mini/issues/161)

### Fixed

- Fix loud clicks when changing bands/modes on the PCB version without the mute circuit. [#103](https://github.com/esp32-si4732/ats-mini/issues/103)
- Do not shadow station names by zoomed menu in the seek mode. [#157](https://github.com/esp32-si4732/ats-mini/issues/157)

## 2.27 (2025-06-07)


### Added

- Allow connecting to the receiver's web UI using the atsmini.local mDNS name in addition to an IP address. [#145](https://github.com/esp32-si4732/ats-mini/issues/145)


### Changed

- Disable Seek mode (menu) timeout.


### Fixed

- Clear RSSI, SNR, and station name when doing normal Seek. [#146](https://github.com/esp32-si4732/ats-mini/issues/146)
- Fix backwards EiBi seek from 30000kHz.

## 2.26 (2025-06-02)


### Added

- Show DHCP-assigned IP address on the About system screen.


### Fixed

- Fix crash when trying download the EiBi schedule in offline mode. [#132](https://github.com/esp32-si4732/ats-mini/issues/132)
- Fix timeout when connecting to Wi-Fi access points.

## 2.25 (2025-05-31)


### Removed

- Disable EEPROM backup/restore option on the settings web page. If you used this feature to restore the EEPROM and now see strange bugs when switching bands, please reset the receiver settings.


### Fixed

- Fix blinking RDS and static frequency name.

## 2.24 (2025-05-30)


### Added

- EiBi schedule support, see https://esp32-si4732.github.io/ats-mini/manual.html#schedule


### Fixed

- Reapply Squelch after waking up from CPU Sleep mode. [#127](https://github.com/esp32-si4732/ats-mini/issues/127)

## 2.23 (2025-05-26)


### Added

- Ability to select FM de-emphasis setting based on region. [#85](https://github.com/esp32-si4732/ats-mini/issues/85)
- Show MAC-address on the receiver status web page. [#114](https://github.com/esp32-si4732/ats-mini/issues/114)
- Add ALL-CT RDS options for those who prefer precise time over WiFi.
- EEPROM backup/restore via the receiver web interface. Restore only works on compatible firmware versions.
- New optional UI layout with large S-meter and S/N-meter.


### Fixed

- Escape quotes in web form fields (like SSIDs & passwords). [#113](https://github.com/esp32-si4732/ats-mini/issues/113)
- Fix the AVC bug, huge thanks to Dave (G8PTN)! [#117](https://github.com/esp32-si4732/ats-mini/issues/117)

## 2.22 (2025-05-23)


### Removed

- Removed the Mute menu option. Use short press instead while in the volume adjustment mode.


### Added

- Experimental Squelch option based on RSSI threshold. Unlikely to work in SSB mode. To turn it off quickly, short press (>0.5 sec) the encoder button in the Squelch menu mode. [#32](https://github.com/esp32-si4732/ats-mini/issues/32)
- Help screen and system info screen (see `Settings->About`).


### Changed

- Use short press to delete a memory slot.


### Fixed

- Fix restoring memory slots that belong to the bands with the same names. [#100](https://github.com/esp32-si4732/ats-mini/issues/100)

## 2.21 (2025-05-21)


### Changed

- Make the Wi-Fi icon a bit more lightweight.


### Fixed

- Disable the automatic tuning capacitor. [#97](https://github.com/esp32-si4732/ats-mini/issues/97)
- NTP time synchronization no longer ignores seconds.

## 2.20 (2025-05-18)


### Added

- Direct frequency input mode.
- `Settings->UTC Offset` now affects the displayed time.
- `Settings->Scroll Dir.` option to reverse the menu scroll direction. [#79](https://github.com/esp32-si4732/ats-mini/issues/79)
- Stop the automatic seek process by clicking the encoder button.
- Use press+rotate for manual fine tuning in Seek mode.
- Wi-Fi mode to sync time over NTP, view the receiver status and Memory slots.


### Fixed

- Set seek step according to the current step. [#5](https://github.com/esp32-si4732/ats-mini/issues/5)
