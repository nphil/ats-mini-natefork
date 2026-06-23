## 276

**BLE OTA: compressed transfer + connection UX fixes**

- BLE firmware/recovery flashing now uses raw-deflate compression (~50% smaller transfer), cutting flash time roughly in half. Firmware inflates on-the-fly using the ESP-ROM tinfl engine; CRC32 is verified over the decompressed bytes. Requires firmware v2.76+ on both sides.
- Fixed OTA state machine getting stuck after a mid-flash BLE disconnect — `remoteOtaAbort()` is now called on disconnect and a 30-second inactivity timeout resets the state, so the next connection gets a clean command dispatcher.
- Fixed duplicate GATT connections accumulating when tapping Connect while already connected; existing GATT client is now closed before opening a new one.
- Android Settings BLE section redesigned: when connected, shows "Connected to [device name]" + Disconnect only (no Scan BLE); when disconnected, shows Scan BLE + device list where only the Connect button is tappable (not the whole row).
- Screen stays on during any flash operation (USB, BLE, Wi-Fi) so the OS cannot dim or lock mid-transfer.
