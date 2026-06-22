**USB Live firmware flashing now completes reliably — added per-block flow control.**
Root cause of the "No completion response" / "boots right back to the old firmware" failures:
the app streamed the whole image with no pacing, overrunning the ESP32-S3's 256-byte USB-CDC
receive buffer, so the radio silently dropped bytes and the transfer never finished. This is
exactly why esptool worked where the app didn't — esptool waits for a reply after every block.
The firmware now grows its USB-CDC RX buffer to 8 KB and **acks every 4 KB block**
(`{"t":"ota","ack":N,"total":T}`); the Android client sends one block, waits for its ack, then
sends the next. The ack also drives the on-screen progress. The radio can no longer be outrun,
so the image lands intact, `esp_ota`/SHA verification passes, and the device reboots into the
new firmware. Applies to both the app-firmware path (`ota_begin`) and the in-place recovery
path (`rec_begin`). Update both sides to v2.71+ / the matching APK for reliable USB flashing.
