// Runtime-settable SNTP server (the "NTP Server" text entity in lglstudio-vfd.yaml).
//
// ESPHome's sntp component sets its servers once, in setup(). On ESP8266 that is the
// SDK's lwIP SNTP client (sntp_stop / sntp_setservername / sntp_init), so re-running the
// same sequence from the main loop swaps the server at runtime. Adapted from
// ~/Projects/esphome-veeb (veeb_clock_ntp.h); no lwIP lock is needed here because on
// ESP8266 lwIP runs in the same (cooperative) context as loop().
//
// The chosen server goes into EVERY lwIP slot: after a timeout lwIP moves to the next
// slot and stays there while it answers, so a different server in slot 1 would quietly
// take over and changing slot 0 would then do nothing. With the same name in all slots a
// timeout just re-resolves it.
#pragma once

#include <cstring>
#include <string>

#include "sntp.h"
#include "esphome/core/log.h"

namespace lglstudio_vfd {

static const char *const NTP_TAG = "lglstudio_vfd.ntp";

// lwIP keeps the pointer, not a copy, so the name must outlive the call.
inline char g_ntp_server[64];

// Hostname or IPv4 address: [A-Za-z0-9.-], 1..63 chars, no leading/trailing '.'/'-'.
inline bool valid_ntp_server(const std::string &s) {
  if (s.empty() || s.size() >= sizeof(g_ntp_server))
    return false;
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-';
    if (!ok)
      return false;
  }
  return s.front() != '.' && s.front() != '-' && s.back() != '.' && s.back() != '-';
}

// Validate and apply. Returns false (and changes nothing) if the name is invalid.
inline bool apply_ntp_server(const std::string &name) {
  if (!valid_ntp_server(name)) {
    ESP_LOGW(NTP_TAG, "Rejected NTP server '%s' (hostname or IPv4 address only)", name.c_str());
    return false;
  }
  // Stop first, then change the buffer lwIP points at.
  sntp_stop();
  strncpy(g_ntp_server, name.c_str(), sizeof(g_ntp_server) - 1);
  g_ntp_server[sizeof(g_ntp_server) - 1] = '\0';
  for (u8_t i = 0; i < SNTP_MAX_SERVERS; i++)
    sntp_setservername(i, g_ntp_server);
  sntp_init();
  ESP_LOGI(NTP_TAG, "NTP server: %s", g_ntp_server);
  return true;
}

}  // namespace lglstudio_vfd
