**USB firmware flashing rebuilt around live serial OTA — reliable on the native USB port.**
The ROM-bootloader approach never rebooted reliably on the ATS-Mini's native USB-Serial/JTAG
(DTR/RTS can't drive GPIO0/EN). USB **Firmware** and **Recovery** now flash *live* over the
running firmware's serial link: the app sends `{"cmd":"ota_begin"}` (or `rec_begin`), streams
the image, and the firmware writes it via `esp_ota`/`Update` — Firmware → the spare OTA slot,
Recovery → the factory partition in place — then reboots itself into the new image. No BOOT
button, no download mode, and the device reliably restarts. JSON/OTA commands are now honoured
over USB regardless of the radio's USB-serial menu setting, so it works out of the box.
A new **Method** switch gives full computer parity: every image (Full / Firmware / Recovery)
can also be flashed via the **ROM bootloader** (esptool-style), which now auto-enters download
mode over the ESP32-S3's native USB-Serial/JTAG using esptool's USB-JTAG reset sequence (BOOT+RESET
still works as a manual fallback) — so you can flash from any state, including a bricked device.
Note: **Live** USB flashing requires firmware v2.66+ already on the radio; for older firmware use
Wi-Fi OTA or the Bootloader method once to get there.

**Live recovery update while already in recovery** is now supported via a power-loss-safe
two-stage self-migration: the recovery firmware receives the new image into the spare OTA slot
(stage 1, CRC-checked), reboots into it, then erases and copies it down to the factory partition
(stage 2), re-verifying by reading the factory partition back. The factory partition is only made
bootable again *after* that read-back CRC matches, so an interrupted copy simply re-runs from the
spare slot on the next boot — it can never leave a half-written, unbootable recovery.

Every flash path is now integrity-checked end-to-end: serial OTA verifies a CRC32 of the
transfer plus `esp_ota`'s SHA-256 image validation; in-place recovery and the migration also
read the written partition back and CRC-verify it; the ROM-bootloader path keeps esptool's
per-block checksums. Image-kind guards block mismatched flashes (e.g. an `ota.bin` selected for a
bootloader Full, or a non-recovery image for Recovery). The serial-OTA client understands both
the main and recovery firmware reply formats. Wi-Fi OTA on the same network is hardened with
smaller flushed chunks and up to 3 retries to survive transient "broken pipe" stalls, plus
cleartext-HTTP support for local radio IPs.
