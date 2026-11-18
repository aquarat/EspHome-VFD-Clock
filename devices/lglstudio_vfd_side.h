// Side-by-side display: a weather carousel on the left, the time right-aligned on the right,
// sent as ONE string per frame. The VFD component assigns CGRAM slots (8 custom glyphs)
// from slot 0 on every print, so printing the halves separately would let the second
// print overwrite the first one's glyphs.
//
// Icons and the one-character wind units are custom 5x7 glyphs; their bitmaps are in the
// `custom:` substitution of lglstudio-vfd.yaml, keyed by the Unicode characters below.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace lglstudio_vfd {

// Glyph keys (must match the `custom:` substitution)
static const char *const GLYPH_TEMP = "🌡";        // thermometer, filled
static const char *const GLYPH_FEELS = "🧍";       // thermometer, hollow ("feels like")
static const char *const GLYPH_HUMIDITY = "🌢";    // droplet
static const char *const GLYPH_WIND = "🌬";        // wind streamlines
static const char *const GLYPH_RAIN = "☂";         // umbrella
static const char *const GLYPH_MPH = "⒨";          // "m" over "h"
static const char *const GLYPH_KMH = "⒦";          // "k" over "h"

// Weather values in display units; NAN = unknown (that item is skipped)
struct SideWeather {
  float temp, feels, humidity, wind, gust, rain;
  bool fahrenheit;
  bool kmh;
};

// Number of display cells: one per UTF-8 code point (each glyph is one cell)
inline size_t cells(const std::string &s) {
  size_t n = 0;
  for (unsigned char c : s)
    n += (c & 0xC0) != 0x80;
  return n;
}

// Rounded, without "-0"
inline long whole(float v) {
  long r = lroundf(v);
  return r == 0 ? 0 : r;
}

// all = false: temperature and wind only; true: every item
inline std::vector<std::string> side_items(const SideWeather &w, bool all) {
  std::vector<std::string> items;
  char b[32];
  const char *deg = w.fahrenheit ? "°F" : "°C";
  const char *unit = w.kmh ? GLYPH_KMH : GLYPH_MPH;
  if (!std::isnan(w.temp)) {
    snprintf(b, sizeof(b), "%s%ld%s", GLYPH_TEMP, whole(w.temp), deg);
    items.push_back(b);
  }
  if (all && !std::isnan(w.feels)) {
    snprintf(b, sizeof(b), "%s%ld%s", GLYPH_FEELS, whole(w.feels), deg);
    items.push_back(b);
  }
  if (all && !std::isnan(w.humidity)) {
    snprintf(b, sizeof(b), "%s%ld%%", GLYPH_HUMIDITY, whole(w.humidity));
    items.push_back(b);
  }
  if (!std::isnan(w.wind)) {
    snprintf(b, sizeof(b), "%s%ld%s", GLYPH_WIND, whole(w.wind), unit);
    items.push_back(b);
  }
  if (all && !std::isnan(w.gust)) {
    snprintf(b, sizeof(b), "%sG%ld%s", GLYPH_WIND, whole(w.gust), unit);
    items.push_back(b);
  }
  if (all && !std::isnan(w.rain)) {
    snprintf(b, sizeof(b), "%s%.1fmm", GLYPH_RAIN, w.rain);
    items.push_back(b);
  }
  return items;
}

// The full line: item `step % items` left-aligned, the time right-aligned, `digits` cells.
// Empty if there is no weather to show (the caller then shows the time alone).
inline std::string side_line(const SideWeather &w, bool all, uint32_t step, const std::string &time,
                             size_t digits) {
  std::vector<std::string> items = side_items(w, all);
  if (items.empty())
    return "";
  std::string left = items[step % items.size()];
  size_t used = cells(left) + cells(time);
  if (used < digits)
    left.append(digits - used, ' ');
  return left + time;
}

}  // namespace lglstudio_vfd
