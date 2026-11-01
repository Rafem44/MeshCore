#pragma once
// GPS of the Cap LoRa-1262 (AT6668, NMEA over UART: ESP RX 15 <- GPS TX, ESP TX 13 -> GPS RX).
// Uses the portable NmeaParser (also run by the simulator). Detects the baud rate by itself,
// keeps the MeshCore clock in sync with GPS time.
#include "../core/Services.h"

class DeviceGps : public GpsService {
public:
  void begin();
  void loop();   // call often: drains the UART (bounded work per call)

  const GpsState& state() override { return _parser.state(); }
  bool present() override;
  uint32_t baud() override { return _locked ? _baud : 0; }
  void coldRestart() override;

private:
  void tryBaud(int idx);
  NmeaParser _parser;
  uint32_t _baud = 0;
  int _baudIdx = 0;
  bool _locked = false;
  uint32_t _tryStart = 0;
  uint32_t _sentencesAtTry = 0;
  uint32_t _lastSentenceMs = 0;
  uint32_t _lastSentences = 0;
  uint32_t _lastClockSync = 0;
  bool _hadFix = false;
};
