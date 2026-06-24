## 277

**Live BLE status sync + interactive spectrum**

- Fixed live telemetry not updating over BLE. On connect the app enabled notifications and then fired several setup commands back-to-back; Android allows only one outstanding GATT operation, so the `sub` command (and others) were silently dropped and the radio never started streaming. All writes now go through a serialized GATT queue, so RSSI / SNR / CPU / battery update continuously and any change made on the radio (frequency, volume, mode, band…) is reflected in the app within ~2 s.
- Fixed the spectrum scan getting stuck near 100% in the app. The firmware now sends the completed scan as a single paced packet (the old ~75 tiny back-to-back BLE notifications overran the link buffer and were dropped) and emits an explicit final progress tick.
- Spectrum chart is now interactive: press and drag across the trace to read the exact frequency and level under your finger, with a callout that follows the cursor. The tuned-frequency marker is drawn as a solid accent line and tracks live status, so it slides in real time as you retune. The strongest bin is flagged and the scanned range is labelled at the edges.
- The spectrum now also appears on the Radio tab (compact, drag-to-inspect) so you can watch the band without leaving the main screen.
- Peak hold / Average controls have a clear active state (filled + check icon); Reset is now a labelled Clear button.
- Requested the 2M BLE PHY on connect (Android 8+) for roughly double throughput, benefiting both streaming and OTA. Firmware flashing over BLE is unaffected — the flash path owns the link exclusively and live streaming is suppressed during a flash.
