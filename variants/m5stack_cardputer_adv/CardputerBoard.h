#pragma once
// M5Stack Cardputer ADV + Cap LoRa-1262 / LoRa868.
// Display, internal I2C (keyboard, codec, Cap IO expander), speaker and battery go through
// M5Unified (vendor library, knows the ADV wiring). Wire is left to the external Grove port.
#include <Arduino.h>
#include <Wire.h>
#include <helpers/ESP32Board.h>

#ifndef PIN_SD_CS
  #define PIN_SD_CS 12
#endif

class CardputerBoard : public ESP32Board {
public:
  void begin();

  // true when the PI4IOE5V6408 expander of the Cap LoRa-1262 answered on the internal I2C bus.
  // false = Cap LoRa868 (no expander) or no Cap at all.
  bool hasCap1262() const { return _cap1262; }

  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override { return "M5Stack Cardputer ADV"; }

private:
  bool probeCapExpander();
  bool enableCapRfPath();
  bool _cap1262 = false;
};
