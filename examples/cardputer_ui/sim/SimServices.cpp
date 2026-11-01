#include "SimServices.h"
#include "../core/RadioPresets.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>

void SimSystem::beep(uint16_t freqHz, uint16_t durationMs) {
  printf("[SIM] beep %u Hz %u ms\n", freqHz, durationMs);
}

bool SimSettings::save(const UiSettings& n) {
  s = n;
  saves++;
  printf("[SIM] settings saved: name='%s' preset=%u tx=%d tz=%d timeout=%u sound=%d ble=%d\n", s.nodeName,
         s.radioPreset, s.txPowerDbm, s.utcOffsetMinutes, s.screenTimeoutSecs, s.sound, s.ble);
  return true;
}

static void copyStr(char* dst, size_t n, const std::string& s) {
  strncpy(dst, s.c_str(), n - 1);
  dst[n - 1] = 0;
  // don't leave a truncated UTF-8 sequence at the end
  size_t len = strlen(dst);
  if (len == n - 1) {
    while (len > 0 && (dst[len - 1] & 0xC0) == 0x80) len--;
    if (len > 0 && (dst[len - 1] & 0x80)) len--;
    dst[len] = 0;
  }
}

void SimMesh::receive(const std::string& sender, const std::string& conversation, const std::string& text, bool isChannel) {
  // Channel messages carry "sender: text" in the payload, like MeshCore group messages.
  _msgs.push_back({conversation, isChannel ? sender + ": " + text : text, _sys.nowUnix(), isChannel, true});
  _rx++;
  hear(sender, 6.5f);
  printf("[SIM] rx %s%s <%s>: %s\n", isChannel ? "#" : "", conversation.c_str(), sender.c_str(), text.c_str());
}

void SimMesh::hear(const std::string& name, float snr) {
  uint32_t now = _sys.millis();
  for (auto& n : _nbs) {
    if (n.name == name) { n.snr = snr; n.heardAt = now; return; }
  }
  _nbs.push_back({name, snr, now});
}

void SimMesh::markAllRead() {
  for (auto& m : _msgs) m.unread = false;
}

int SimMesh::unreadCount() {
  return (int)std::count_if(_msgs.begin(), _msgs.end(), [](const Msg& m) { return m.unread; });
}

int SimMesh::recentMessages(MessagePreview* out, int max) {
  int n = 0;
  for (auto it = _msgs.rbegin(); it != _msgs.rend() && n < max; ++it, ++n) {
    copyStr(out[n].from, sizeof(out[n].from), it->from);
    copyStr(out[n].text, sizeof(out[n].text), it->text);
    out[n].timestamp = it->ts;
    out[n].isChannel = it->channel;
    out[n].unread = it->unread;
  }
  return n;
}

int SimMesh::neighbours(NeighbourInfo* out, int max) {
  std::vector<Nb> sorted = _nbs;
  std::sort(sorted.begin(), sorted.end(), [](const Nb& a, const Nb& b) { return a.heardAt > b.heardAt; });
  int n = 0;
  uint32_t now = _sys.millis();
  for (auto& nb : sorted) {
    if (n >= max) break;
    copyStr(out[n].name, sizeof(out[n].name), nb.name);
    out[n].snr = nb.snr;
    out[n].lastHeardSecs = (now - nb.heardAt) / 1000;
    n++;
  }
  return n;
}

RadioStats SimMesh::radioStats() {
  const UiSettings& s = _settings.get();
  const RadioPreset& p = RADIO_PRESETS[s.radioPreset < RADIO_PRESET_COUNT ? s.radioPreset : 0];
  RadioStats r;
  r.rxPackets = _rx;
  r.freqMHz = p.freqMHz;
  r.bandwidthKHz = p.bwKHz;
  r.spreadingFactor = p.sf;
  r.txPowerDbm = s.txPowerDbm;
  r.lastSnr = 6.5f;
  r.lastRssi = -92;
  return r;
}
