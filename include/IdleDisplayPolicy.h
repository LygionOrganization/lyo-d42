#pragma once
#include <atomic>
#include <cstdint>
// External control traffic only; cached HTTP telemetry does not use the motor bus.
inline std::atomic<uint32_t> lastControlActivity{0};
inline bool idleDisplayDue(uint32_t now,uint32_t refreshed,uint32_t activity) {
  return uint32_t(now-refreshed)>=60000 && uint32_t(now-activity)>=1000;
}
