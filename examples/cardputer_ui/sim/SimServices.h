#pragma once
// Fake implementations of the UI services for the PC simulator.
// Everything is driven by the scenario script (see main_sim.cpp), with a virtual clock
// so that screenshots are reproducible. Messages are stored with the real MessageStore,
// on host files, so persistence and reboots are tested for real.
#include "../core/Services.h"
#include <deque>
#include <string>
#include <vector>

// MessageStore backend on host files (one directory per scenario).
class SimStorage : public Storage {
public:
  explicit SimStorage(std::string dir) : _dir(std::move(dir)) {}
  bool read(const char* path, uint32_t off, void* buf, uint32_t len) override;
  bool write(const char* path, uint32_t off, const void* buf, uint32_t len) override;
  bool remove(const char* path) override;
  uint32_t writes = 0;

private:
  std::string full(const char* path) const;
  std::string _dir;
};

class SimGps;

class SimSystem : public SystemService {
public:
  uint32_t virtualMillis = 0;
  DeviceStatus st;
  bool soundOn = true;
  SimGps* gps = nullptr;

  SimSystem() { st.batteryPercent = 82; }
  uint32_t millis() override { return virtualMillis; }
  DeviceStatus status() override;
  uint32_t freeHeap() override { return 180 * 1024; }
  uint32_t minFreeHeap() override { return 171 * 1024; }
  uint16_t batteryMilliVolts() override { return 3600 + st.batteryPercent * 6; }
  const char* firmwareVersion() override { return "sim-dev"; }
  void beep(uint16_t freqHz, uint16_t durationMs) override;
  void playSound(SoundKind kind) override;

  uint32_t nowUnix() { return st.unixTime ? st.unixTime + virtualMillis / 1000 : 0; }
  int sounds = 0;
};

// GPS fed with NMEA sentences by the scenario (same parser as on the device).
class SimGps : public GpsService {
public:
  explicit SimGps(SimSystem& sys) : _sys(sys) {}
  bool feed(const std::string& sentence) { _present = true; _quick = false; return _parser.feedLine(sentence.c_str(), _sys.millis()); }
  void quick(const std::string& mode, int sats);   // "fix" (at the scenario position), "nofix", "none"
  const GpsState& state() override { return _quick ? _q : _parser.state(); }
  bool present() override { return _present; }
  uint32_t baud() override { return _present ? 115200 : 0; }
  void coldRestart() override { _parser.reset(); _quick = false; printf("[SIM] GPS cold restart\n"); }

private:
  SimSystem& _sys;
  NmeaParser _parser;
  GpsState _q;
  bool _quick = false;
  bool _present = false;
};

// Map tiles from a host folder (same layout as the SD card: <dir>/tiles/<z>/<x>/<y>.bin).
class SimTiles : public TileSource {
public:
  bool open(const std::string& dir);   // reads <dir>/tiles/info.txt
  bool available() override { return _ok; }
  bool zoomRange(int& zmin, int& zmax) override { zmin = _zmin; zmax = _zmax; return _ok; }
  bool hint(double& lat, double& lon) override { lat = _lat; lon = _lon; return _ok; }
  bool beginTile(int z, int x, int y) override;
  bool readRow(int row, int x0, int w, uint16_t* dst) override;
  void endTile() override;
  int tilesRead = 0, rowsRead = 0;

private:
  std::string _dir;
  bool _ok = false;
  int _zmin = 0, _zmax = 0;
  double _lat = 0, _lon = 0;
  FILE* _f = nullptr;
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
  SimMesh(SimSystem& sys, SimSettings& settings, Storage& storage);

  // ---- scenario hooks ----
  void addContact(const std::string& name, int hops, bool fav, uint8_t type, bool hasPos, double lat, double lon);
  void receive(const std::string& sender, const std::string& text);                           // direct
  void receiveChannel(const std::string& channel, const std::string& sender, const std::string& text);
  void hear(const std::string& name, float snr);
  void reboot();              // flush + reload the message store from the files
  void tick();                // advances the fake network (ACKs, retries)
  bool netAcks = true;        // contacts acknowledge direct messages
  uint32_t netDelayMs = 1500; // ACK round trip
  bool radioBusy = false;     // sendText fails (radio queue full)
  int sentPackets = 0;
  ConvKey keyByName(const std::string& name);   // conversation or contact by name

  // ---- MeshService ----
  const char* selfName() override { return _settings.get().nodeName; }
  MessageStore& messages() override { return _store; }
  bool sendText(const ConvKey& to, const char* text) override;
  int maxTextLen(const ConvKey& to) override;
  int contactCount() override { return (int)_contacts.size(); }
  bool contactAt(int i, ContactView& out) override;
  bool findContact(const ConvKey& key, ContactView& out) override;
  bool setFavourite(const ConvKey& key, bool fav) override;
  bool removeContact(const ConvKey& key) override;
  bool resetPath(const ConvKey& key) override;
  bool sendAdvert(bool flood, bool withPosition) override;
  GpsService* gps = nullptr;
  int advertsWithPosition = 0;
  bool joinChannel(const char* name) override;
  bool leaveChannel(const ConvKey& key) override;
  bool pollEvent(MeshEvent& out) override;
  int neighbours(NeighbourInfo* out, int max) override;
  RadioStats radioStats() override;

private:
  struct Contact { ContactView v; uint32_t heardAtMs; float snr; };
  struct Pending { ConvKey key; uint32_t seq; uint32_t deadline; uint32_t ackAt; };
  struct Channel { ConvKey key; std::string name; };

  Contact* findMut(const ConvKey& k);
  void sortContacts();
  void push(MeshEvent::Type t, const ConvKey& k, const std::string& title, const std::string& text);
  void transmit(Pending& p, StoredMsg& m);

  SimSystem& _sys;
  SimSettings& _settings;
  MessageStore _store;
  Storage& _storage;
  std::vector<Contact> _contacts;
  std::vector<Channel> _channels;
  std::vector<Pending> _pending;
  std::deque<MeshEvent> _events;
  uint32_t _rx = 0;
};
