#include "CardputerBoard.h"
#include <M5Unified.h>

// PI4IOE5V6408 IO expander on the Cap LoRa-1262 (internal I2C bus, SDA 8 / SCL 9).
static constexpr uint8_t PI4IOE_ADDR = 0x43;
static constexpr uint8_t PI4IOE_REG_ID = 0x01;       // bits 7..5 = manufacturer ID (0b101)
static constexpr uint8_t PI4IOE_REG_DIR = 0x03;      // 1 = output
static constexpr uint8_t PI4IOE_REG_OUT = 0x05;      // output level
static constexpr uint8_t PI4IOE_REG_HIZ = 0x07;      // 1 = output high-impedance (reset value 0xFF)
static constexpr uint8_t PI4IOE_CAP_PIN = 0;         // P0 enables the Cap's RF front-end
static constexpr uint32_t I2C_FREQ = 400000;

#ifndef CPADV_CAP_POWER_PIN
  // Some Cap revisions are reported to need GPIO46 high before the expander answers.
  // Only driven if the expander does not answer at first.
  #define CPADV_CAP_POWER_PIN 46
#endif

void CardputerBoard::begin() {
  Serial.println("[CPADV] board begin");

  // The SD card shares the radio SPI bus (SCK 40 / MOSI 14 / MISO 39): keep its CS released
  // so an inserted card never answers to radio traffic.
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  ESP32Board::begin();   // Wire on the Grove port (PIN_BOARD_SDA / PIN_BOARD_SCL)

  auto cfg = M5.config();
  cfg.serial_baudrate = 0;      // Serial already started by main
  cfg.clear_display = true;
  cfg.output_power = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_spk = true;
  cfg.external_imu = false;
  cfg.external_rtc = false;
  cfg.fallback_board = m5::board_t::board_M5CardputerADV;
  M5.begin(cfg);

  Serial.printf("[CPADV] M5Unified board id=%d (ADV=%d), display %dx%d\n",
                (int)M5.getBoard(), (int)m5::board_t::board_M5CardputerADV,
                (int)M5.Display.width(), (int)M5.Display.height());
  if (M5.getBoard() != m5::board_t::board_M5CardputerADV) {
    Serial.println("[CPADV] WARNING: board not detected as Cardputer ADV");
  }

  _cap1262 = probeCapExpander();
#if CPADV_CAP_POWER_PIN >= 0
  if (!_cap1262) {
    Serial.printf("[CPADV] PI4IOE not found, driving GPIO%d high and retrying\n", CPADV_CAP_POWER_PIN);
    pinMode(CPADV_CAP_POWER_PIN, OUTPUT);
    digitalWrite(CPADV_CAP_POWER_PIN, HIGH);
    delay(100);
    _cap1262 = probeCapExpander();
  }
#endif
  if (_cap1262) {
    Serial.println("[CPADV] Cap LoRa-1262 detected (PI4IOE @0x43)");
    if (!enableCapRfPath()) Serial.println("[CPADV] ERROR: could not configure PI4IOE P0");
  } else {
    Serial.println("[CPADV] no PI4IOE: assuming Cap LoRa868 (DIO2 RF switch only)");
  }

  Serial.printf("[CPADV] battery %u mV\n", getBattMilliVolts());
}

bool CardputerBoard::probeCapExpander() {
  uint8_t id = 0;
  if (!M5.In_I2C.readRegister(PI4IOE_ADDR, PI4IOE_REG_ID, &id, 1, I2C_FREQ)) return false;
  Serial.printf("[CPADV] PI4IOE id register 0x%02X (manufacturer bits %u)\n", id, id >> 5);
  return true;
}

bool CardputerBoard::enableCapRfPath() {
  const uint8_t bit = 1 << PI4IOE_CAP_PIN;
  bool ok = true;
  // Order matters: set the level first, then make it an output, then leave high-impedance.
  ok &= M5.In_I2C.bitOn(PI4IOE_ADDR, PI4IOE_REG_OUT, bit, I2C_FREQ);
  ok &= M5.In_I2C.bitOn(PI4IOE_ADDR, PI4IOE_REG_DIR, bit, I2C_FREQ);
  ok &= M5.In_I2C.bitOff(PI4IOE_ADDR, PI4IOE_REG_HIZ, bit, I2C_FREQ);
  delay(5);
  uint8_t dir = M5.In_I2C.readRegister8(PI4IOE_ADDR, PI4IOE_REG_DIR, I2C_FREQ);
  uint8_t out = M5.In_I2C.readRegister8(PI4IOE_ADDR, PI4IOE_REG_OUT, I2C_FREQ);
  uint8_t hiz = M5.In_I2C.readRegister8(PI4IOE_ADDR, PI4IOE_REG_HIZ, I2C_FREQ);
  Serial.printf("[CPADV] PI4IOE dir=0x%02X out=0x%02X hiz=0x%02X -> P0 %s\n", dir, out, hiz,
                ((dir & bit) && (out & bit) && !(hiz & bit)) ? "HIGH (ok)" : "NOT SET");
  return ok;
}

uint16_t CardputerBoard::getBattMilliVolts() {
  int mv = M5.Power.getBatteryVoltage();   // ADC on GPIO10, divider handled by M5Unified
  return mv > 0 ? (uint16_t)mv : 0;
}
