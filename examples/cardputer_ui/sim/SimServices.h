#pragma once
// Fake implementations of the UI services for the PC simulator.
// Everything is driven by the scenario script (see main_sim.cpp), with a virtual clock
// so that screenshots are reproducible.
#include "../core/Services.h"
#include <string>
#include <vector>

class SimSystem : public SystemService {
public:
  uint32_t virtualMillis = 0;
  DeviceStatus st;

  SimSystem() {
    st.batteryPercent = 82;
    st.bleEnabled = false;
    st.unixTime = 0;
  }
  uint32_t millis() override { return virtualMillis; }
  DeviceStatus status() override {
    DeviceStatus s = st;
    if (s.unixTime) s.unixTime += virtualMillis / 1000;
    return s;
  }
  uint32_t freeHeap() override { return 180 * 1024; }
  uint32_t minFreeHeap() override { return 171 * 1024; }
  uint16_t batteryMilliVolts() override { return 3600 + st.batteryPercent * 6; }
  const char* firmwareVersion() override { return "sim-dev"; }
  void beep(uint16_t freqHz, uint16_t durationMs) override;

  uint32_t nowUnix() { return st.unixTime ? st.unixTime + virtualMillis / 1000 : virtualMillis / 1000; }
};

class SimSettings : public SettingsService {
public:
  UiSettings s;
  int saves = 0;
  const UiSettings& get() override { return s; }
  bool save(const UiSettings& n) override;
};

class SimMesh : public MeshService {
public:
  SimMesh(SimSystem& sys, SimSettings& settings) : _sys(sys), _settings(settings) {}

  // Scenario hooks
  // `conversation` is the contact name for a direct message, or the channel name.
  void receive(const std::string& sender, const std::string& conversation, const std::string& text, bool isChannel);
  void hear(const std::string& name, float snr);
  void markAllRead();

  // MeshService
  const char* selfName() override { return _settings.get().nodeName; }
  int unreadCount() override;
  int recentMessages(MessagePreview* out, int max) override;
  int neighbours(NeighbourInfo* out, int max) override;
  RadioStats radioStats() override;

private:
  struct Msg { std::string from, text; uint32_t ts; bool channel, unread; };
  struct Nb { std::string name; float snr; uint32_t heardAt; };
  SimSystem& _sys;
  SimSettings& _settings;
  std::vector<Msg> _msgs;
  std::vector<Nb> _nbs;
  uint32_t _rx = 0;
};
