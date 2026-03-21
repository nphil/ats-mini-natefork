#include "Common.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

uint8_t cpuDisplayIdx = 0;
uint8_t cpuFreqIdx = 0;

static volatile uint32_t idleCount[2] = {0, 0};
static uint8_t cpuLoad[2] = {0, 0};

// Low-priority task that simply counts iterations — runs only when
// the core has nothing more important to do, measuring idle time.
static void idleTask(void *param)
{
  const int core = *(const int *)param;
  while (true) {
    idleCount[core]++;
    taskYIELD();
  }
}

static const int coreNum[2] = {0, 1};

void cpuInitTasks()
{
  xTaskCreatePinnedToCore(idleTask, "cpuIdle0", 2048, (void *)&coreNum[0], 0, NULL, 0);
  xTaskCreatePinnedToCore(idleTask, "cpuIdle1", 2048, (void *)&coreNum[1], 0, NULL, 1);
}

// Call once per main loop; returns true when values have been updated (every ~1 s).
bool cpuUpdateLoad()
{
  static uint32_t lastCount[2] = {0, 0};
  static uint32_t maxCount[2]  = {1UL, 1UL};  // starts at 1 so first sample snaps it to real idle rate
  static uint32_t lastMs = 0;

  uint32_t now = millis();
  if (now - lastMs < 1000) return false;
  uint32_t elapsed = now - lastMs;
  lastMs = now;

  for (int i = 0; i < 2; i++) {
    uint32_t cur    = idleCount[i];
    uint32_t delta  = cur - lastCount[i];
    lastCount[i]    = cur;

    // Normalise to counts-per-second
    uint32_t perSec = (uint32_t)((uint64_t)delta * 1000 / elapsed);

    // Running-max baseline: snaps up instantly, decays 1 %/s
    if (perSec >= maxCount[i]) {
      maxCount[i] = perSec;
    } else {
      maxCount[i] -= maxCount[i] / 100;
      if (maxCount[i] < perSec) maxCount[i] = perSec;
    }
    if (maxCount[i] < 1) maxCount[i] = 1;

    uint32_t usagePct = (maxCount[i] > perSec)
                        ? ((maxCount[i] - perSec) * 100 / maxCount[i])
                        : 0;
    cpuLoad[i] = (uint8_t)(usagePct > 100 ? 100 : usagePct);
  }
  return true;
}

uint8_t getCpuLoad(int core)
{
  return cpuLoad[core & 1];
}
