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
Wi-Fi OTA or the Bootloader method once to get there. **Live** serial flashing handles Firmware & Recovery while
the radio runs (in normal firmware, or Firmware while in recovery). Image-kind guards block
mismatched flashes (e.g. an `ota.bin` selected for a bootloader Full, or a non-recovery image
for Recovery). The serial-OTA client now understands both the main and recovery firmware reply
formats, so live Firmware updates work whether the radio is in normal or recovery mode. Wi-Fi
OTA on the same network is hardened with smaller flushed chunks and up to 3 retries to survive
transient "broken pipe" stalls, plus cleartext-HTTP support for local radio IPs.
