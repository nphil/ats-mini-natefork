**USB flashing now works from normal firmware, recovery, and bootloader modes.**
The DTR/RTS auto-reset sequence had a critical timing bug: GPIO0 (download-mode select)
was released before reset was de-asserted, so the chip booted normally into recovery
instead of ROM download mode. Fixed by holding GPIO0 low through the reset release.
A new `{"cmd":"reboot_dl"}` remote command (added to both main and recovery firmware)
sets the RTC force-download flag before restarting, giving a software path into the
ROM bootloader that doesn't require the BOOT button. The Android app now tries the
soft command first, then falls back to DTR/RTS, waits for USB re-enumeration, and
reconnects automatically. Cleartext HTTP to local radio IPs is now permitted (fixes
"cleartext not permitted" error on same-network Wi-Fi OTA). Image-kind validation
guards prevent cross-partition accidents: flashing a recovery image without selecting
Recovery kind (or vice versa) is blocked with an explanatory warning, and Wi-Fi OTA
warns if a non-OTA binary is selected.
