**Fixed: USB Live flashing from recovery mode hung on "Verifying & finalizing".**
When the radio was already in recovery mode, flashing firmware (or a recovery image) over USB with the
app's Live flasher would stream, then stall forever at finalize. The recovery firmware's serial-OTA
handler never got the flow-control fix the main firmware has: its begin reply omitted the block size,
so the app fell back to blasting the image with no pacing, and recovery's default 256-byte USB-CDC RX
buffer dropped bytes — so the received byte count never reached the expected size and finalization
never ran.

Recovery now mirrors the main firmware exactly: it grows the RX buffer to 8 KB, advertises a 4 KB
block size, and acks every block (`{"t":"ota","ack":N,"total":T}`) so the app waits for each ack
before sending the next — the same request/response discipline esptool uses. It also emits a
`{"fin":1}` heartbeat before the slow SHA-256 verify so the app keeps waiting through finalization.
Live USB flashing now works the same from recovery as from the running firmware. Requires re-flashing
the recovery partition (USB **Recovery** in-place, or the Bootloader method).
