#include <Arduino.h>
#include "target.h"

CardputerBoard board;

// Radio SPI: the SD card shares this bus (CS 12, kept high by CardputerBoard::begin).
// The display uses its own SPI host (SPI3), configured by M5GFX.
static SPIClass spi;
SPIClass& radio_spi() { return spi; }
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
MicroNMEALocationProvider gps(Serial1, &rtc_clock);
EnvironmentSensorManager sensors(gps);

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  CardputerKeyButton user_btn(CardputerKeyButton::ENTER, PIN_USER_BTN);   // keyboard Enter + G0 button
  CardputerKeyButton joystick_left(CardputerKeyButton::LEFT);
  CardputerKeyButton joystick_right(CardputerKeyButton::RIGHT);
  CardputerKeyButton back_btn(CardputerKeyButton::BACK);
#endif

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);

  Serial.printf("[CPADV] SX1262 init: NSS=%d DIO1=%d RST=%d BUSY=%d SCK=%d MISO=%d MOSI=%d TCXO=%.1fV DIO2_RF_SWITCH=%d\n",
                P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI,
                (float)SX126X_DIO3_TCXO_VOLTAGE, (int)SX126X_DIO2_AS_RF_SWITCH);
  Serial.printf("[CPADV] Cap variant: %s\n", board.hasCap1262() ? "LoRa-1262 (PI4IOE)" : "LoRa868 / unknown");

  bool ok = radio.std_init(&spi);
  if (ok) {
    Serial.printf("[CPADV] SX1262 OK: %.3f MHz BW %.1f SF %d, TX %d dBm\n",
                  (float)LORA_FREQ, (float)LORA_BW, (int)LORA_SF, (int)LORA_TX_POWER);
  } else {
    Serial.println("[CPADV] ERROR: SX1262 init failed (see 'radio init failed' code above)");
    Serial.println("[CPADV] hints: -2 = chip not found (wiring/NSS/BUSY), -707/-706 = TCXO voltage");
  }
  return ok;
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}
