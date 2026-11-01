#pragma once
// Cardputer ADV keyboard: TCA8418 on the internal I2C bus (0x34, SDA 8 / SCL 9, INT 11).
// I2C goes through M5.In_I2C (shared with the codec and the Cap IO expander).
// Raw events are decoded by core/KeyMatrix (also exercised by the simulator).
#include "../core/KeyMatrix.h"

class Tca8418Keyboard {
public:
  bool begin();
  // Reads pending key events; returns true and fills `out` for the next key press.
  // Call repeatedly until it returns false.
  bool poll(KeyEvent& out);
  bool ok() const { return _ok; }
  uint32_t eventsRead() const { return _events; }

private:
  bool readReg(uint8_t reg, uint8_t& v);
  bool writeReg(uint8_t reg, uint8_t v);

  KeyMatrix _matrix;
  bool _ok = false;
  uint32_t _lastPoll = 0;
  uint32_t _events = 0;
  uint8_t _pending[10];
  uint8_t _pendingCount = 0, _pendingPos = 0;
};
