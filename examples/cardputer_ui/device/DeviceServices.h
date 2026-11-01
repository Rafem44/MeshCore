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

private:
  DeviceSettings& _settings;
};

// Phase 2: no mesh yet (messaging arrives in phase 3). Reports the node name and the real
// radio counters so the home and system screens show live data.
class DeviceMeshStub : public MeshService {
public:
  DeviceMeshStub(DeviceSettings& settings, bool& radioReady) : _settings(settings), _radioReady(radioReady) {}
  const char* selfName() override { return _settings.get().nodeName; }
  int unreadCount() override { return 0; }
  int recentMessages(MessagePreview*, int) override { return 0; }
  int neighbours(NeighbourInfo*, int) override { return 0; }
  RadioStats radioStats() override;

private:
  DeviceSettings& _settings;
  bool& _radioReady;
};
