#include "SimServices.h"
#include "../core/RadioPresets.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>

// ------------------------------------------------------------------ SimStorage

std::string SimStorage::full(const char* path) const { return _dir + path; }

bool SimStorage::read(const char* path, uint32_t off, void* buf, uint32_t len) {
  FILE* f = fopen(full(path).c_str(), "rb");
  if (!f) return false;
  bool ok = fseek(f, off, SEEK_SET) == 0 && fread(buf, 1, len, f) == len;
  fclose(f);
  return ok;
}

bool SimStorage::write(const char* path, uint32_t off, const void* buf, uint32_t len) {
  std::string p = full(path);
  FILE* f = fopen(p.c_str(), "r+b");
  if (!f) f = fopen(p.c_str(), "w+b");
  if (!f) return false;
  bool ok = fseek(f, off, SEEK_SET) == 0 && fwrite(buf, 1, len, f) == len;
  fclose(f);
  writes++;
  return ok;
}

bool SimStorage::remove(const char* path) { return ::remove(full(path).c_str()) == 0; }

// ------------------------------------------------------------------ SimSystem / SimSettings

void SimSystem::beep(uint16_t freqHz, uint16_t durationMs) {
  printf("[SIM] beep %u Hz %u ms\n", freqHz, durationMs);
}

void SimSystem::playSound(SoundKind kind) {
  static const char* const names[] = {"direct", "channel", "sent", "fail"};
  sounds++;
  printf("[SIM] sound %s\n", names[kind]);
}

bool SimSettings::save(const UiSettings& n) {
  s = n;
  saves++;
  printf("[SIM] settings saved: name='%s' preset=%u tx=%d tz=%d timeout=%u sound=%d ble=%d\n", s.nodeName,
         s.radioPreset, s.txPowerDbm, s.utcOffsetMinutes, s.screenTimeoutSecs, s.sound, s.ble);
  return true;
}

// ------------------------------------------------------------------ SimMesh

static ConvKey keyFromName(uint8_t kind, const std::string& name) {
  // Stable fake "public key prefix": FNV-1a of the name
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : name) { h ^= c; h *= 1099511628211ull; }
  ConvKey k;
  k.kind = kind;
  for (int i = 0; i < 6; i++) k.id[i] = (h >> (i * 8)) & 0xFF;
  return k;
}

SimMesh::SimMesh(SimSystem& sys, SimSettings& settings, Storage& storage)
    : _sys(sys), _settings(settings), _storage(storage) {
  _store.begin(&_storage);
  joinChannel("Public");
}

void SimMesh::reboot() {
  _store.flush();
  _pending.clear();
  _events.clear();
  _store.begin(&_storage);
  printf("[SIM] reboot: %d conversation(s) reloaded, %d unread\n", _store.count(), _store.unreadTotal());
}

SimMesh::Contact* SimMesh::findMut(const ConvKey& k) {
  for (auto& c : _contacts) if (c.v.key == k) return &c;
  return nullptr;
}

ConvKey SimMesh::keyByName(const std::string& name) {
  for (auto& ch : _channels) if (ch.name == name) return ch.key;
  for (auto& c : _contacts) if (name == c.v.name) return c.v.key;
  for (int i = 0; i < _store.count(); i++) if (name == _store.at(i)->name) return _store.at(i)->key;
  return ConvKey();
}

void SimMesh::sortContacts() {
  std::stable_sort(_contacts.begin(), _contacts.end(), [](const Contact& a, const Contact& b) {
    if (a.v.favourite != b.v.favourite) return a.v.favourite;
    return a.v.lastHeard > b.v.lastHeard;
  });
}

void SimMesh::push(MeshEvent::Type t, const ConvKey& k, const std::string& title, const std::string& text) {
  MeshEvent e;
  e.type = t;
  e.key = k;
  utf8Copy(e.title, sizeof(e.title), title.c_str());
  utf8Copy(e.text, sizeof(e.text), text.c_str());
  _events.push_back(e);
}

void SimMesh::addContact(const std::string& name, int hops, bool fav, uint8_t type, bool hasPos, double lat,
                         double lon) {
  ConvKey k = keyFromName(ConvKey::DIRECT, name);
  Contact* c = findMut(k);
  if (!c) { _contacts.push_back(Contact()); c = &_contacts.back(); }
  c->v = ContactView{};
  c->v.key = k;
  memcpy(c->v.pubKey, k.id, 6);
  utf8Copy(c->v.name, sizeof(c->v.name), name.c_str());
  c->v.type = type;
  c->v.favourite = fav;
  c->v.hops = hops;
  c->v.lastHeard = _sys.nowUnix();
  c->v.hasPos = hasPos;
  c->v.latE6 = (int32_t)(lat * 1e6);
  c->v.lonE6 = (int32_t)(lon * 1e6);
  c->heardAtMs = _sys.millis();
  c->snr = 7.25f;
  sortContacts();
}

void SimMesh::hear(const std::string& name, float snr) {
  ConvKey k = keyFromName(ConvKey::DIRECT, name);
  Contact* c = findMut(k);
  if (!c) {
    addContact(name, 1, false, CONTACT_CHAT, false, 0, 0);
    push(MeshEvent::CONTACT_NEW, k, name, "");
    c = findMut(k);
  }
  c->snr = snr;
  c->heardAtMs = _sys.millis();
  c->v.lastHeard = _sys.nowUnix();
  sortContacts();
}

void SimMesh::receive(const std::string& sender, const std::string& text) {
  hear(sender, 6.5f);
  ConvKey k = keyFromName(ConvKey::DIRECT, sender);
  Contact* c = findMut(k);
  StoredMsg m;
  m.ts = _sys.nowUnix();
  m.state = MSG_RECEIVED;
  m.snrX4 = 26;
  m.hops = c ? std::max<int>(0, c->v.hops) : 1;
  utf8Copy(m.text, sizeof(m.text), text.c_str());
  _store.append(k, sender.c_str(), m, true);
  _rx++;
  push(MeshEvent::MESSAGE, k, sender, text);
  printf("[SIM] rx %s: %s\n", sender.c_str(), text.c_str());
}

void SimMesh::receiveChannel(const std::string& channel, const std::string& sender, const std::string& text) {
  ConvKey k = keyByName(channel);
  if (!k.valid() || k.kind != ConvKey::CHANNEL) {
    printf("[SIM] rx on unknown channel %s ignored (not joined)\n", channel.c_str());
    return;
  }
  StoredMsg m;
  m.ts = _sys.nowUnix();
  m.state = MSG_RECEIVED;
  m.snrX4 = 18;
  m.hops = 2;
  utf8Copy(m.sender, sizeof(m.sender), sender.c_str());
  utf8Copy(m.text, sizeof(m.text), text.c_str());
  _store.append(k, channel.c_str(), m, true);
  _rx++;
  push(MeshEvent::MESSAGE, k, channel, sender + ": " + text);
  printf("[SIM] rx #%s <%s>: %s\n", channel.c_str(), sender.c_str(), text.c_str());
}

int SimMesh::maxTextLen(const ConvKey& to) {
  if (to.kind == ConvKey::CHANNEL) return MSG_TEXT_MAX - (int)strlen(selfName()) - 2;
  return MSG_TEXT_MAX;
}

void SimMesh::transmit(Pending& p, StoredMsg& m) {
  m.attempt++;
  Contact* c = findMut(p.key);
  m.state = (c && c->v.hops >= 0) ? MSG_SENT_DIRECT : MSG_SENT_FLOOD;
  p.deadline = _sys.millis() + 4000;
  p.ackAt = netAcks ? _sys.millis() + netDelayMs : 0;
  sentPackets++;
  printf("[SIM] tx attempt %u to %s (%s)\n", m.attempt, c ? c->v.name : "?",
         m.state == MSG_SENT_DIRECT ? "direct" : "flood");
}

bool SimMesh::sendText(const ConvKey& to, const char* text) {
  if (radioBusy) return false;
  StoredMsg m;
  m.ts = _sys.nowUnix();
  m.outgoing = 1;
  utf8Copy(m.text, (size_t)maxTextLen(to) + 1, text);
  const ConvMeta* meta = _store.find(to);
  std::string name = meta ? meta->name : "";
  if (to.kind == ConvKey::CHANNEL) {
    m.state = MSG_CHANNEL_SENT;
    sentPackets++;
    return _store.append(to, name.c_str(), m, false) >= 0;
  }
  Contact* c = findMut(to);
  if (!c) return false;
  Pending p{to, 0, 0, 0};
  transmit(p, m);
  int32_t seq = _store.append(to, c->v.name, m, false);
  if (seq < 0) return false;
  p.seq = seq;
  _pending.push_back(p);
  return true;
}

void SimMesh::tick() {
  uint32_t now = _sys.millis();
  for (size_t i = 0; i < _pending.size();) {
    Pending& p = _pending[i];
    StoredMsg m;
    if (!_store.read(p.key, p.seq, m)) { _pending.erase(_pending.begin() + i); continue; }
    const ConvMeta* meta = _store.find(p.key);
    std::string name = meta ? meta->name : "?";
    if (p.ackAt && (int32_t)(now - p.ackAt) >= 0) {
      m.state = MSG_DELIVERED;
      _store.update(p.key, p.seq, m);
      push(MeshEvent::DELIVERED, p.key, name, m.text);
      _pending.erase(_pending.begin() + i);
      continue;
    }
    if ((int32_t)(now - p.deadline) >= 0) {
      if (m.attempt < 3) {
        if (m.attempt == 2) {   // last chance: forget the path, flood
          if (Contact* c = findMut(p.key)) c->v.hops = -1;
        }
        transmit(p, m);
        _store.update(p.key, p.seq, m);
      } else {
        m.state = MSG_FAILED;
        _store.update(p.key, p.seq, m);
        push(MeshEvent::FAILED, p.key, name, m.text);
        _pending.erase(_pending.begin() + i);
        continue;
      }
    }
    i++;
  }
  _store.flush();
}

bool SimMesh::contactAt(int i, ContactView& out) {
  if (i < 0 || i >= (int)_contacts.size()) return false;
  out = _contacts[i].v;
  return true;
}

bool SimMesh::findContact(const ConvKey& key, ContactView& out) {
  Contact* c = findMut(key);
  if (!c) return false;
  out = c->v;
  return true;
}

bool SimMesh::setFavourite(const ConvKey& key, bool fav) {
  Contact* c = findMut(key);
  if (!c) return false;
  c->v.favourite = fav;
  sortContacts();
  return true;
}

bool SimMesh::removeContact(const ConvKey& key) {
  auto it = std::find_if(_contacts.begin(), _contacts.end(), [&](const Contact& c) { return c.v.key == key; });
  if (it == _contacts.end()) return false;
  _contacts.erase(it);
  return true;
}

bool SimMesh::resetPath(const ConvKey& key) {
  Contact* c = findMut(key);
  if (!c) return false;
  c->v.hops = -1;
  return true;
}

bool SimMesh::sendAdvert(bool flood) {
  sentPackets++;
  printf("[SIM] advert (%s)\n", flood ? "flood" : "zero hop");
  return true;
}

bool SimMesh::joinChannel(const char* name) {
  for (auto& ch : _channels) if (ch.name == name) return true;
  if (_channels.size() >= 8) return false;
  Channel ch{keyFromName(ConvKey::CHANNEL, name), name};
  _channels.push_back(ch);
  _store.ensure(ch.key, name);
  return true;
}

bool SimMesh::leaveChannel(const ConvKey& key) {
  auto it = std::find_if(_channels.begin(), _channels.end(), [&](const Channel& c) { return c.key == key; });
  if (it == _channels.end()) return false;
  _channels.erase(it);
  _store.remove(key);
  return true;
}

bool SimMesh::pollEvent(MeshEvent& out) {
  if (_events.empty()) return false;
  out = _events.front();
  _events.pop_front();
  return true;
}

int SimMesh::neighbours(NeighbourInfo* out, int max) {
  std::vector<const Contact*> v;
  for (auto& c : _contacts) v.push_back(&c);
  std::sort(v.begin(), v.end(), [](const Contact* a, const Contact* b) { return a->heardAtMs > b->heardAtMs; });
  int n = 0;
  for (auto* c : v) {
    if (n >= max) break;
    utf8Copy(out[n].name, sizeof(out[n].name), c->v.name);
    out[n].snr = c->snr;
    out[n].lastHeardSecs = (_sys.millis() - c->heardAtMs) / 1000;
    n++;
  }
  return n;
}

RadioStats SimMesh::radioStats() {
  const UiSettings& s = _settings.get();
  const RadioPreset& p = RADIO_PRESETS[s.radioPreset < RADIO_PRESET_COUNT ? s.radioPreset : 0];
  RadioStats r;
  r.rxPackets = _rx;
  r.txPackets = sentPackets;
  r.freqMHz = p.freqMHz;
  r.bandwidthKHz = p.bwKHz;
  r.spreadingFactor = p.sf;
  r.txPowerDbm = s.txPowerDbm;
  r.lastSnr = 6.5f;
  r.lastRssi = -92;
  return r;
}
