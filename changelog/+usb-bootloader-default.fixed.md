**USB flashing now defaults to the reliable ROM-bootloader path (the same method esptool uses).**
Diagnosis: on the ATS-Mini's native ESP32-S3 USB-Serial/JTAG, the device→host direction
(serial console) and the ROM download mode (esptool) both work, but the host→device direction
*into the running firmware* is hardware-flaky — which is exactly what the "Live" OTA path relies
on, so it can stall with "no response from radio." The ROM bootloader is a separate hardware
download mode that esptool drives directly and that the app's **Bootloader** method already
mirrors block-for-block. The USB-flash panel now defaults to **Bootloader (esptool)** for
Firmware/Recovery/Full — reliable from any state (normal, recovery, or bricked) after a manual
download-mode entry (hold BOOT, tap RESET, release BOOT). **Live** remains available as a
no-buttons convenience for radios whose USB receive path cooperates, but it's no longer the
default and its on-screen description now states the trade-off plainly.
