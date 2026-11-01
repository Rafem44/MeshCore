#include "Tca8418Keyboard.h"
#include <Arduino.h>
#include <M5Unified.h>

namespace {
constexpr uint8_t ADDR = 0x34;
constexpr uint32_t FREQ = 400000;
constexpr uint8_t PIN_INT = 11;
constexpr uint8_t REG_CFG = 0x01, REG_INT_STAT = 0x02, REG_KEY_LCK_EC = 0x03, REG_KEY_EVENT_A = 0x04;
constexpr uint8_t REG_KP_GPIO_1 = 0x1D, REG_KP_GPIO_2 = 0x1E, REG_KP_GPIO_3 = 0x1F;
constexpr uint8_t REG_GPI_EM_1 = 0x20, REG_GPIO_DIR_1 = 0x23, REG_GPIO_INT_EN_1 = 0x1A;
constexpr uint8_t CFG_KE_IEN = 0x01;
}  // namespace

bool Tca8418Keyboard::readReg(uint8_t reg, uint8_t& v) {
  return M5.In_I2C.readRegister(ADDR, reg, &v, 1, FREQ);
}

bool Tca8418Keyboard::writeReg(uint8_t reg, uint8_t v) {
  return M5.In_I2C.writeRegister8(ADDR, reg, v, FREQ);
}

bool Tca8418Keyboard::begin() {
  // Same configuration as M5Stack's driver: 7 rows x 8 columns keypad, key events only.
  bool ok = true;
  for (uint8_t i = 0; i < 3; i++) {
    ok &= writeReg(REG_GPIO_DIR_1 + i, 0x00);
    ok &= writeReg(REG_GPI_EM_1 + i, 0xFF);
    ok &= writeReg(REG_GPIO_INT_EN_1 + i, 0x00);
  }
  ok &= writeReg(REG_KP_GPIO_1, 0x7F);   // rows 0..6
  ok &= writeReg(REG_KP_GPIO_2, 0xFF);   // columns 0..7
  ok &= writeReg(REG_KP_GPIO_3, 0x00);
  ok &= writeReg(REG_CFG, CFG_KE_IEN);

  // Flush anything that happened during boot
  uint8_t ev = 0;
  int flushed = 0;
  while (readReg(REG_KEY_EVENT_A, ev) && ev && flushed < 16) flushed++;
  writeReg(REG_INT_STAT, 0x03);

  pinMode(PIN_INT, INPUT_PULLUP);
  _ok = ok;
  Serial.printf("[CPADV] TCA8418 keyboard %s (flushed %d events)\n", ok ? "ready" : "NOT RESPONDING", flushed);
  return ok;
}

bool Tca8418Keyboard::poll(KeyEvent& out) {
  if (!_ok) return false;

  for (;;) {
    // Decode events already fetched from the chip
    while (_pendingPos < _pendingCount) {
      uint8_t ev = _pending[_pendingPos++];
      if (_matrix.feed(ev, out)) return true;
    }
    // Fetch new events: the INT line (active low) avoids useless I2C traffic,
    // with a slow fallback poll in case the line is not wired as expected.
    uint32_t now = millis();
    bool intActive = digitalRead(PIN_INT) == LOW;
    if (!intActive && now - _lastPoll < 50) return false;
    _lastPoll = now;

    uint8_t count = 0;
    if (!readReg(REG_KEY_LCK_EC, count)) return false;
    count &= 0x0F;
    if (count == 0) {
      if (intActive) writeReg(REG_INT_STAT, 0x03);
      return false;
    }
    _pendingCount = 0;
    _pendingPos = 0;
    for (uint8_t i = 0; i < count && i < sizeof(_pending); i++) {
      uint8_t ev = 0;
      if (!readReg(REG_KEY_EVENT_A, ev) || ev == 0) break;
      _pending[_pendingCount++] = ev;
      _events++;
    }
    writeReg(REG_INT_STAT, 0x03);
    if (_pendingCount == 0) return false;
  }
}
