#ifndef RECOVERY_H
#define RECOVERY_H

// Called at the very top of setup() before any other init.
// If the encoder button is held at power-on for >1 second, enters a
// self-contained recovery mode (WiFi AP + HTTP OTA) and never returns.
void checkRecoveryBoot();

#endif // RECOVERY_H
