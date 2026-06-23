**Republish: recovery USB flow-control fix now ships in a binary.**
v2.73 was published by a release race (two pushes landed within minutes, both resolved to the same
version), so its recovery binary was built from the earlier commit and missed the serial-OTA
flow-control fix. This release rebuilds from main so the fix is actually included in the published
`*-ospi-recovery.bin`. No code change versus the intended v2.73 — purely a corrected release.
