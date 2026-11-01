#pragma once
// Cardputer implementations of the UI services (core/Services.h).
#include "../core/Services.h"

#ifndef CPADV_VERSION
  #define CPADV_VERSION "0.2.0"
#endif

// Settings stored in LittleFS (/cpadv_settings.bin), applied to the radio on save.
class DeviceSettings : public SettingsService {
public:
  void load();
  void applyRadio();             // pushes preset + TX power to the SX1262
  void setRadioReady(bool ok) { _radioReady = ok; }
  const UiSettings& get() override { return _s; }
  bool save(const UiSettings& s) override;

private:
  UiSettings _s;
  bool _radioReady = false;
};

class DeviceSystem : public SystemService {
public:
  explicit DeviceSystem(DeviceSettings& settings) : _settings(settings) {}
  uint32_t millis() override;
  DeviceStatus status() override;
  uint32_t freeHeap() override;
  uint32_t minFreeHeap() override;
  uint16_t batteryMilliVolts() override;
  const char* firmwareVersion() override { return CPADV_VERSION; }
  void beep(uint16_t freqHz, uint16_t durationMs) override;
  void playSound(SoundKind kind) override;

private:
  DeviceSettings& _settings;
};
