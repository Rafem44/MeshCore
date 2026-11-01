#pragma once
// The only boundary between the UI and the rest of the firmware.
// - Hardware build: implemented on top of MeshCore (BaseChatMesh), the GPS UART, the battery ADC...
// - Simulator build: implemented by sim/SimServices.* (fake mesh, NMEA replay, virtual clock).
// The UI must never include Arduino, RadioLib or MeshCore headers directly.
#include <stdint.h>
#include <stddef.h>

struct DeviceStatus {
  int  batteryPercent = -1;   // -1 = unknown
  bool charging = false;
  bool gpsFix = false;
  int  gpsSatellites = 0;
  bool bleEnabled = false;
  bool bleConnected = false;
  uint32_t unixTime = 0;      // 0 = time not known yet (no GPS fix, no phone sync)
  int  utcOffsetMinutes = 60; // local time offset, from settings
};

struct RadioStats {
  uint32_t rxPackets = 0;
  uint32_t txPackets = 0;
  uint32_t rxErrors = 0;
  float lastRssi = 0;
  float lastSnr = 0;
  float freqMHz = 0;
  float bandwidthKHz = 0;
  uint8_t spreadingFactor = 0;
  int8_t txPowerDbm = 0;
};

struct NeighbourInfo {
  char name[32];
  float snr;
  uint32_t lastHeardSecs;     // seconds since last heard
};

struct MessagePreview {
  char from[32];              // contact name, or channel name for group messages
  char text[96];              // UTF-8, possibly truncated
  uint32_t timestamp;         // unix time
  bool isChannel;
  bool unread;
};

class MeshService {
public:
  virtual ~MeshService() {}
  virtual const char* selfName() = 0;
  virtual int unreadCount() = 0;
  // Fills `out` with up to `max` items, newest first. Returns the count written.
  virtual int recentMessages(MessagePreview* out, int max) = 0;
  virtual int neighbours(NeighbourInfo* out, int max) = 0;
  virtual RadioStats radioStats() = 0;
};

class SystemService {
public:
  virtual ~SystemService() {}
  virtual uint32_t millis() = 0;
  virtual DeviceStatus status() = 0;
  virtual uint32_t freeHeap() = 0;
  virtual void beep(uint16_t freqHz, uint16_t durationMs) = 0;
};

struct Services {
  MeshService* mesh;
  SystemService* sys;
};
