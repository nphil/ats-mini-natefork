## ATS-Mini Remote 3.2 — Silent auto-reconnect

The app now remembers the last ATS-Mini you connected to and quietly reconnects whenever it comes back online — no more manually tapping the Bluetooth pill after the radio sleeps, reboots, or finishes its 5-minute BLE auto-off cycle.

### Auto-reconnect
- The peripheral UUID of the last connected radio is persisted to UserDefaults
- On launch (or whenever Bluetooth becomes available), the app does a fast `retrievePeripherals` lookup and connects immediately if the radio is already discoverable
- If not, it falls into a quiet background scan and connects the moment the radio re-advertises
- Involuntary disconnects (radio sleeps, out of range, firmware's 5-min BLE auto-off) trigger the same background scan automatically
- Tapping **Disconnect** explicitly forgets the saved peripheral so the app won't fight you and reconnect

### Paired firmware update (v234)
Best paired with ATS-Mini firmware **v234** which:
- Always enables BLE at every boot (5 min, then auto-off at runtime if no client connects)
- Fixes a crash when the radio enters light sleep with a BLE client still connected
- Ships 30 new themes matching the iOS app palette (Homebox, Dark, Forest, Ocean, Dracula, Synthwave, Cyberpunk, etc.) so the device and the app look consistent

---

Unsigned IPA — sideload with AltStore / SideStore / Feather.
