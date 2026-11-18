// Runtime POSIX time zones (the "Main/Alt Time Zone POSIX" text entities).
//
// ESPHome 2026.10 removed RealTimeClock::set_timezone(const char*): time zones are now
// parsed at codegen time (aioesphomeapi.posix_tz) into a time::ParsedTimezone, and one
// global zone is used by every time component (time::set_global_tz). Nothing parses a
// string on the device any more, so this is a C++ port of aioesphomeapi's
// parse_posix_tz(), field for field, to keep trip5's editable time zones working.
//
// Only the homeassistant time platform also calls set_global_tz (from the API); this
// config uses sntp + rx8025, so the zone set here stays put.
#pragma once

#include <cctype>
#include <string>

#include "esphome/components/time/posix_tz.h"
#include "esphome/core/log.h"

namespace lglstudio_vfd {

static const char *const TZ_TAG = "lglstudio_vfd.tz";

namespace tz_detail {

using esphome::time::DSTRule;
using esphome::time::DSTRuleType;

// Each returns false on a parse error, leaving pos where it stopped.
inline bool parse_uint(const std::string &s, size_t &pos, int32_t &out) {
  size_t start = pos;
  out = 0;
  while (pos < s.size() && isdigit((unsigned char) s[pos]))
    out = out * 10 + (s[pos++] - '0');
  return pos != start;
}

inline bool skip_name(const std::string &s, size_t &pos) {
  if (pos < s.size() && s[pos] == '<') {
    while (++pos < s.size() && s[pos] != '>') {
    }
    if (pos >= s.size())
      return false;  // unterminated <...>
    pos++;
    return true;
  }
  size_t start = pos;
  while (pos < s.size() && isalpha((unsigned char) s[pos]))
    pos++;
  return pos - start >= 3;
}

// [+-]hh[:mm[:ss]] in seconds
inline bool parse_offset(const std::string &s, size_t &pos, int32_t &out) {
  int32_t sign = 1, h, m = 0, sec = 0;
  if (pos < s.size() && (s[pos] == '-' || s[pos] == '+'))
    sign = s[pos++] == '-' ? -1 : 1;
  if (!parse_uint(s, pos, h))
    return false;
  if (pos < s.size() && s[pos] == ':') {
    pos++;
    if (!parse_uint(s, pos, m))
      return false;
    if (pos < s.size() && s[pos] == ':') {
      pos++;
      if (!parse_uint(s, pos, sec))
        return false;
    }
  }
  out = sign * (h * 3600 + m * 60 + sec);
  return true;
}

// Mm.w.d[/time], Jn[/time] or n[/time]
inline bool parse_rule(const std::string &s, size_t &pos, DSTRule &rule) {
  int32_t v;
  rule = DSTRule{};
  if (pos >= s.size())
    return false;
  char c = s[pos];
  if (c == 'M' || c == 'm') {
    rule.type = DSTRuleType::MONTH_WEEK_DAY;
    pos++;
    if (!parse_uint(s, pos, v) || v < 1 || v > 12)
      return false;
    rule.month = v;
    if (pos >= s.size() || s[pos++] != '.' || !parse_uint(s, pos, v) || v < 1 || v > 5)
      return false;
    rule.week = v;
    if (pos >= s.size() || s[pos++] != '.' || !parse_uint(s, pos, v) || v > 6)
      return false;
    rule.day_of_week = v;
  } else if (c == 'J' || c == 'j') {
    rule.type = DSTRuleType::JULIAN_NO_LEAP;
    pos++;
    if (!parse_uint(s, pos, v) || v < 1 || v > 365)
      return false;
    rule.day = v;
  } else if (isdigit((unsigned char) c)) {
    rule.type = DSTRuleType::DAY_OF_YEAR;
    if (!parse_uint(s, pos, v) || v > 365)
      return false;
    rule.day = v;
  } else {
    return false;
  }
  rule.time_seconds = 2 * 3600;  // default 02:00
  if (pos < s.size() && s[pos] == '/') {
    pos++;
    if (!parse_offset(s, pos, rule.time_seconds))
      return false;
  }
  return true;
}

}  // namespace tz_detail

// Same results as aioesphomeapi.posix_tz.parse_posix_tz(); false where it raises.
inline bool parse_posix_tz(const std::string &s, esphome::time::ParsedTimezone &tz) {
  using namespace tz_detail;
  tz = esphome::time::ParsedTimezone{};
  size_t pos = 0;
  if (s.empty() || !skip_name(s, pos))
    return false;
  if (pos >= s.size() || !(isdigit((unsigned char) s[pos]) || s[pos] == '+' || s[pos] == '-'))
    return false;
  if (!parse_offset(s, pos, tz.std_offset_seconds))
    return false;
  if (pos >= s.size())
    return true;  // no DST
  if (s[pos] == ',')
    return false;  // rules without a DST name
  if (!isalpha((unsigned char) s[pos]) && s[pos] != '<')
    return true;  // no DST, trailing characters ignored
  if (!skip_name(s, pos))
    return false;
  if (pos < s.size() && s[pos] != ',' && (isdigit((unsigned char) s[pos]) || s[pos] == '+' || s[pos] == '-')) {
    if (!parse_offset(s, pos, tz.dst_offset_seconds))
      return false;
  } else {
    tz.dst_offset_seconds = tz.std_offset_seconds - 3600;
  }
  if (pos >= s.size() || s[pos] != ',')
    return true;  // DST name without rules: no DST
  pos++;
  if (!parse_rule(s, pos, tz.dst_start))
    return false;
  if (pos >= s.size() || s[pos] != ',')
    return false;  // second rule is required
  pos++;
  return parse_rule(s, pos, tz.dst_end);
}

// Parse and make it the global zone. Returns false (and changes nothing) if invalid.
inline bool apply_timezone(const std::string &posix) {
  esphome::time::ParsedTimezone tz;
  if (!parse_posix_tz(posix, tz)) {
    ESP_LOGW(TZ_TAG, "Rejected time zone '%s' (not a POSIX TZ string)", posix.c_str());
    return false;
  }
  esphome::time::set_global_tz(tz);
  ESP_LOGI(TZ_TAG, "Time zone: %s", posix.c_str());
  return true;
}

}  // namespace lglstudio_vfd
