// Auto brightness: day/night levels with linear ramps at sunrise and sunset.
//
// The ramp up starts at sunrise + sunrise offset and reaches the day level `ramp` minutes
// later; the ramp down starts at sunset + sunset offset and reaches the night level `ramp`
// minutes later. Offsets may be negative (start before the event).
//
// ESPHome's sun component returns the NEXT sunrise/sunset, but a window that opened before
// an event (negative offset) or is still ramping after one needs the events either side
// of now, so the caller passes several days of events. Each sunrise is paired with the
// first sunset after it (not the sunset of the same UTC day: far east of Greenwich that
// one comes first) to make a daylight window; the level is the brightest window at `now`,
// which stays right across midnight, with large offsets and when the ramps overlap.
#pragma once

#include <algorithm>
#include <cstdint>
#include <ctime>

namespace lglstudio_vfd {

// 0..1 progress of a ramp that starts at `start` and lasts `ramp_s` seconds (0 = a step)
inline float ramp_progress(time_t now, time_t start, int32_t ramp_s) {
  if (ramp_s <= 0)
    return now >= start ? 1.0f : 0.0f;
  return std::clamp(float(now - start) / float(ramp_s), 0.0f, 1.0f);
}

// Daylight factor 0 (night) .. 1 (day) at `now`. `rises`/`sets` are event times (any order,
// 0 = none). `fallback_day` (sun above the horizon) is used when no window can be formed
// (polar day or night).
inline float daylight_factor(time_t now, const time_t *rises, int n_rises, const time_t *sets, int n_sets,
                             int32_t rise_offset_s, int32_t set_offset_s, int32_t ramp_s, bool fallback_day) {
  bool any = false;
  float best = 0.0f;
  for (int i = 0; i < n_rises; i++) {
    if (rises[i] == 0)
      continue;
    time_t set = 0;  // first sunset after this sunrise
    for (int j = 0; j < n_sets; j++)
      if (sets[j] > rises[i] && (set == 0 || sets[j] < set))
        set = sets[j];
    if (set == 0)
      continue;
    any = true;
    float up = ramp_progress(now, rises[i] + rise_offset_s, ramp_s);
    float down = 1.0f - ramp_progress(now, set + set_offset_s, ramp_s);
    best = std::max(best, std::min(up, down));
  }
  return any ? best : (fallback_day ? 1.0f : 0.0f);
}

inline uint8_t auto_level(float daylight_f, float day_level, float night_level) {
  float v = night_level + (day_level - night_level) * daylight_f;
  return (uint8_t) std::clamp(v + 0.5f, 0.0f, 255.0f);
}

}  // namespace lglstudio_vfd
