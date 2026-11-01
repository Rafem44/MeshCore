#pragma once
// The only boundary between the UI and the rest of the firmware.
// - Hardware build: implemented on top of MeshCore (BaseChatMesh), the GPS UART, the battery ADC...
// - Simulator build: implemented by sim/SimServices.* (fake mesh, NMEA replay, virtual clock).
// The UI must never include Arduino, RadioLib or MeshCore headers directly.
#include <stdint.h>
#include <stddef.h>
#include "MessageStore.h"

struct DeviceStatus {
  int  batteryPercent = -1;   // -1 = unknown
  bool charging = false;
  bool gpsFix = false;
  int  gpsSatellites = 0;
  bool bleEnabled = false;
  bool bleConnected = false;
  uint32_t unixTime = 0;      // 0 = time not known yet (no GPS fix, no phone sync)
  bool hasPosition = false;   // own position (GPS fix or manual)
  double lat = 0, lon = 0;
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

enum ContactType : uint8_t { CONTACT_CHAT = 1, CONTACT_REPEATER = 2, CONTACT_ROOM = 3, CONTACT_SENSOR = 4 };

struct ContactView {
  ConvKey key;                // DIRECT + public key prefix: also the conversation key
  uint8_t pubKey[32];
  char name[32];
  uint8_t type;               // ContactType
  bool favourite;
  int8_t hops;                // -1: no known path (messages are flooded)
  uint32_t lastHeard;         // unix time (our clock) of the last advert, 0 = never
  bool hasPos;
  int32_t latE6, lonE6;       // position from the contact's advert (degrees * 1e6)
};

struct MeshEvent {
  enum Type : uint8_t { NONE = 0, MESSAGE, DELIVERED, FAILED, CONTACT_NEW };
  Type type = NONE;
  ConvKey key;
  char title[32] = {};        // conversation / contact name
  char text[64] = {};         // message preview
};

class MeshService {
public:
  virtual ~MeshService() {}
  virtual const char* selfName() = 0;

  // Conversations and history (read by the screens, written by the mesh).
  virtual MessageStore& messages() = 0;
  int unreadCount() { return messages().unreadTotal(); }
  // Stores the message and sends it (direct with ACK + retries, or to a channel).
  virtual bool sendText(const ConvKey& to, const char* text) = 0;
  // Max text length in bytes for this conversation (channels: name prefix included).
  virtual int maxTextLen(const ConvKey& to) = 0;

  // Contacts: favourites first, then most recently heard.
  virtual int contactCount() = 0;
  virtual bool contactAt(int i, ContactView& out) = 0;
  virtual bool findContact(const ConvKey& key, ContactView& out) = 0;
  virtual bool setFavourite(const ConvKey& key, bool fav) = 0;
  virtual bool removeContact(const ConvKey& key) = 0;
  virtual bool resetPath(const ConvKey& key) = 0;

  virtual bool sendAdvert(bool flood) = 0;
  // Joins a hashtag channel ("#name", key derived from the name, like the MeshCore app).
  virtual bool joinChannel(const char* name) = 0;
  virtual bool leaveChannel(const ConvKey& key) = 0;

  // Events for notifications (new message, delivery...). Returns false when empty.
  virtual bool pollEvent(MeshEvent& out) = 0;

  virtual int neighbours(NeighbourInfo* out, int max) = 0;
  virtual RadioStats radioStats() = 0;
};

enum SoundKind : uint8_t { SOUND_DIRECT_MSG, SOUND_CHANNEL_MSG, SOUND_SENT, SOUND_FAIL };

class SystemService {
public:
  virtual ~SystemService() {}
  virtual uint32_t millis() = 0;
  virtual DeviceStatus status() = 0;
  virtual uint32_t freeHeap() = 0;
  virtual uint32_t minFreeHeap() = 0;          // lowest free heap since boot
  virtual uint16_t batteryMilliVolts() = 0;
  virtual const char* firmwareVersion() = 0;
  virtual void beep(uint16_t freqHz, uint16_t durationMs) = 0;
  // Short notification jingles (respect the "Son" setting).
  virtual void playSound(SoundKind kind) = 0;
};

// User settings edited on the device. Persisted by the platform (LittleFS on the Cardputer).
struct UiSettings {
  char nodeName[32] = "Cardputer";
  uint8_t radioPreset = 0;          // index in RADIO_PRESETS (core/RadioPresets.h)
  int8_t txPowerDbm = 22;
  int16_t utcOffsetMinutes = 60;    // France: +60 in winter, +120 in summer
  uint16_t screenTimeoutSecs = 60;  // 0 = never
  bool sound = true;
  bool ble = false;
};

class SettingsService {
public:
  virtual ~SettingsService() {}
  virtual const UiSettings& get() = 0;
  // Stores and applies the new settings (radio parameters, name...). Returns false on error.
  virtual bool save(const UiSettings& s) = 0;
};

struct Services {
  MeshService* mesh;
  SystemService* sys;
  SettingsService* settings;
};
