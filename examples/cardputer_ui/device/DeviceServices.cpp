#include "DeviceServices.h"
#include "../core/RadioPresets.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <target.h>

extern MicroNMEALocationProvider gps;   // defined in variants/m5stack_cardputer_adv/target.cpp

namespace {
constexpr const char* SETTINGS_PATH = "/cpadv_settings.bin";
constexpr uint32_t SETTINGS_MAGIC = 0x43504144;   // "CPAD"
constexpr uint16_t SETTINGS_VERSION = 1;

struct SettingsFile {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  UiSettings s;
  uint32_t checksum;
};

uint32_t checksum(const uint8_t* p, size_t n) {
  uint32_t h = 2166136261u;   // FNV-1a
  for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}

const RadioPreset& presetOf(const UiSettings& s) {
  return RADIO_PRESETS[s.radioPreset < RADIO_PRESET_COUNT ? s.radioPreset : 0];
}
}  // namespace

// ------------------------------------------------------------------ DeviceSettings

void DeviceSettings::load() {
  File f = LittleFS.open(SETTINGS_PATH, "r");
  if (!f) {
    Serial.println("[CPADV] settings: no file, using defaults");
    return;
  }
  SettingsFile sf;
  size_t n = f.read((uint8_t*)&sf, sizeof(sf));
  f.close();
  if (n != sizeof(sf) || sf.magic != SETTINGS_MAGIC || sf.version != SETTINGS_VERSION ||
      sf.size != sizeof(UiSettings) || sf.checksum != checksum((const uint8_t*)&sf.s, sizeof(sf.s))) {
    Serial.println("[CPADV] settings: file invalid or from another version, using defaults");
    return;
  }
  _s = sf.s;
  _s.nodeName[sizeof(_s.nodeName) - 1] = 0;
  Serial.printf("[CPADV] settings loaded: name='%s' preset=%u tx=%d\n", _s.nodeName, _s.radioPreset, _s.txPowerDbm);
}

bool DeviceSettings::save(const UiSettings& s) {
  bool radioChanged = s.radioPreset != _s.radioPreset || s.txPowerDbm != _s.txPowerDbm;
  _s = s;
  SettingsFile sf;
  sf.magic = SETTINGS_MAGIC;
  sf.version = SETTINGS_VERSION;
  sf.size = sizeof(UiSettings);
  sf.s = _s;
  sf.checksum = checksum((const uint8_t*)&sf.s, sizeof(sf.s));
  File f = LittleFS.open(SETTINGS_PATH, "w");
  bool ok = f && f.write((const uint8_t*)&sf, sizeof(sf)) == sizeof(sf);
  if (f) f.close();
  Serial.printf("[CPADV] settings saved (%s)\n", ok ? "ok" : "WRITE FAILED");
  if (radioChanged) applyRadio();
  return ok;
}

void DeviceSettings::applyRadio() {
  if (!_radioReady) return;
  const RadioPreset& p = presetOf(_s);
  radio_driver.setParams(p.freqMHz, p.bwKHz, p.sf, p.cr);
  radio_driver.setTxPower(_s.txPowerDbm);
  Serial.printf("[CPADV] radio set: %.3f MHz BW %.1f SF %u CR %u, %d dBm\n", p.freqMHz, p.bwKHz, p.sf, p.cr,
                _s.txPowerDbm);
}

// ------------------------------------------------------------------ DeviceSystem

uint32_t DeviceSystem::millis() { return ::millis(); }

DeviceStatus DeviceSystem::status() {
  DeviceStatus st;
  st.batteryPercent = M5.Power.getBatteryLevel();
  st.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  st.gpsFix = gps.isValid();
  st.gpsSatellites = gps.satellitesCount();
  st.bleEnabled = _settings.get().ble;
  st.bleConnected = false;
  uint32_t t = rtc_clock.getCurrentTime();
  st.unixTime = t > 1700000000u ? t : 0;   // ignore the clock until it has been set (GPS / app)
  return st;
}

uint32_t DeviceSystem::freeHeap() { return ESP.getFreeHeap(); }
uint32_t DeviceSystem::minFreeHeap() { return ESP.getMinFreeHeap(); }
uint16_t DeviceSystem::batteryMilliVolts() { return board.getBattMilliVolts(); }

void DeviceSystem::beep(uint16_t freqHz, uint16_t durationMs) {
  if (!_settings.get().sound) return;
  M5.Speaker.tone(freqHz, durationMs);
}

// ------------------------------------------------------------------ DeviceMeshStub

RadioStats DeviceMeshStub::radioStats() {
  const UiSettings& s = _settings.get();
  const RadioPreset& p = presetOf(s);
  RadioStats r;
  r.freqMHz = p.freqMHz;
  r.bandwidthKHz = p.bwKHz;
  r.spreadingFactor = p.sf;
  r.txPowerDbm = s.txPowerDbm;
  if (_radioReady) {
    r.rxPackets = radio_driver.getPacketsRecv();
    r.txPackets = radio_driver.getPacketsSent();
    r.rxErrors = radio_driver.getPacketsRecvErrors();
    r.lastRssi = radio_driver.getLastRSSI();
    r.lastSnr = radio_driver.getLastSNR();
  }
  return r;
}
