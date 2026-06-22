**USB Live flashing now pre-flights the serial link and tells you exactly what's wrong.**
Live USB flashing talks to the *running* firmware over USB serial (unlike esptool, which
drives the ROM bootloader). Before streaming a multi-MB image the app now sends a
`{"cmd":"ping"}` and waits for `{"t":"pong"}` — confirming two-way USB comms are actually
alive and the firmware is new enough to speak the OTA protocol. If the link is dead you get a
clear "serial link isn't answering — reconnect / use a data cable / fall back to Bootloader
mode" message instead of a silent stall, and a too-old firmware is now reported distinctly
from a dead link. The begin handshake also retries more times with a longer per-try window.
Both the main and recovery firmware answer `ping`, so the probe works in either state.
