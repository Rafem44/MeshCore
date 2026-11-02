#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <CardputerBoard.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#include <helpers/sensors/MicroNMEALocationProvider.h>
#ifdef DISPLAY_CLASS
  #include <CardputerDisplay.h>
  #include <CardputerKeys.h>
#endif

extern CardputerBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  // Keyboard-driven "buttons" for the companion UI (UI_HAS_JOYSTICK): see CardputerKeys.h
  extern CardputerKeyButton user_btn;
  extern CardputerKeyButton joystick_left;
  extern CardputerKeyButton joystick_right;
  extern CardputerKeyButton back_btn;
#endif

bool radio_init();
// SPI bus shared by the SX1262 and the SD card (SCK 40 / MOSI 14 / MISO 39)
SPIClass& radio_spi();
mesh::LocalIdentity radio_new_identity();
