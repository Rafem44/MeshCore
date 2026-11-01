#include "DeviceGps.h"
#if !CPADV_DEMO
#include <Arduino.h>
#include <target.h>

namespace {
// AT6668 / ATGM336H default to 115200 on the Cap; other common rates as fallback.
const uint32_t BAUDS[] = {115200, 9600, 38400};
constexpr int BAUD_COUNT = sizeof(BAUDS) / sizeof(BAUDS[0]);
constexpr uint32_t BAUD_TRY_MS = 2500;
}  // namespace

void DeviceGps::tryBaud(int idx) {
  _baudIdx = idx % BAUD_COUNT;
  _baud = BAUDS[_baudIdx];
  Serial1.end();
  // MeshCore naming: PIN_GPS_TX is the GPS's TX line (= our RX)
  Serial1.begin(_baud, SERIAL_8N1, PIN_GPS_TX, PIN_GPS_RX);
  _tryStart = millis();
  _sentencesAtTry = _parser.state().sentences;
  Serial.printf("[CPADV] GPS: listening at %u baud (RX %d, TX %d)\n", (unsigned)_baud, PIN_GPS_TX, PIN_GPS_RX);
}

void DeviceGps::begin() {
  Serial1.setRxBufferSize(1024);   // a full NMEA second is ~500 bytes
  tryBaud(0);
}

bool DeviceGps::present() {
  return _locked && millis() - _lastSentenceMs < 5000;
}

void DeviceGps::coldRestart() {
  char cmd[32];
  size_t n = NmeaParser::frame("PCAS10,2", cmd, sizeof(cmd));   // CASIC cold start (AT6668 / ATGM336H)
  Serial1.write((const uint8_t*)cmd, n);
  _parser.reset();
  _hadFix = false;
  Serial.printf("[CPADV] GPS: cold restart sent: %s", cmd);
}

void DeviceGps::loop() {
  uint32_t now = millis();
  int budget = 512;   // bytes per call: never starve the radio
  while (budget-- && Serial1.available()) _parser.feed((char)Serial1.read(), now);

  const GpsState& s = _parser.state();
  if (s.sentences != _lastSentences) {
    _lastSentences = s.sentences;
    _lastSentenceMs = now;
    if (!_locked) {
      _locked = true;
      Serial.printf("[CPADV] GPS: NMEA detected at %u baud\n", (unsigned)_baud);
    }
  }
  if (!_locked && now - _tryStart > BAUD_TRY_MS) {
    if (s.checksumErrors) Serial.printf("[CPADV] GPS: %u checksum errors at %u baud\n", (unsigned)s.checksumErrors, (unsigned)_baud);
    tryBaud(_baudIdx + 1);
  }

  if (s.valid != _hadFix) {
    _hadFix = s.valid;
    if (s.valid) Serial.printf("[CPADV] GPS: fix %.5f, %.5f, %u sats, hdop %.1f\n", s.lat, s.lon, s.sats, s.hdop);
    else Serial.println("[CPADV] GPS: fix lost");
  }

  // Keep the MeshCore clock on GPS time (message timestamps, "seen x min ago")
  if (s.utcUnix > 1735689600u && now - _lastSentenceMs < 2000 && now - _lastClockSync > 60000) {
    uint32_t rtc = rtc_clock.getCurrentTime();
    int32_t drift = (int32_t)(s.utcUnix - rtc);
    if (drift > 2 || drift < -2) {
      rtc_clock.setCurrentTime(s.utcUnix);
      Serial.printf("[CPADV] clock set from GPS (drift %d s)\n", (int)drift);
    }
    _lastClockSync = now;
  }
}

#endif  // !CPADV_DEMO
