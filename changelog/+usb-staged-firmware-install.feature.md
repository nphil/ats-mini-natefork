**USB firmware updates now install themselves through recovery — the persistent, brick-safe path.**
On this board every boot routes through recovery, which always forwards to `ota_0`, and the main
app erases otadata on boot — so an OTA that switches to the *other* slot runs once then reverts.
That's why USB updates never "stuck". USB **Firmware** flashing now matches how phones update:
the app streams the image (flow-controlled, CRC-checked) into the spare `ota_1` slot, the radio
records the size+CRC and reboots straight into recovery (no countdown), and recovery installs it
to `ota_0` — the slot the device actually boots — verifying the image (`esp_ota` SHA-256) and a
CRC read-back before switching, then reboots into the new firmware. It's the same proven install
the Wi-Fi recovery OTA already uses, just sourced over USB.

The whole flow is power-loss safe and can't brick: the image is staged in `ota_1` without touching
the running firmware, recovery only marks `ota_0` bootable after it verifies, an interrupted install
re-runs from the intact staged image on the next boot, and a persistently failing install falls back
to the recovery menu with `ota_0` untouched. Requires **both** v2.72 main firmware *and* v2.72
recovery (the installer lives in recovery); update recovery once via USB **Recovery** (in-place) or
the Bootloader method. USB **Recovery** flashing still writes the factory partition in place. The
Android USB-flash panel defaults back to this no-buttons path, with Bootloader (esptool) one tap away.
