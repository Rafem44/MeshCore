// Standalone Cardputer ADV firmware (phase 2): keyboard UI, settings, system screen.
// Messaging (phase 3), GPS screens (phase 4) and maps (phase 5) plug into the same App.
#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <target.h>

#include "../core/App.h"
#include "../core/screens/HomeScreen.h"
#include "../core/screens/SettingsScreens.h"
#include "DeviceServices.h"
#include "Tca8418Keyboard.h"

static M5Canvas canvas(&M5.Display);   // the single 240x135 RGB565 framebuffer (64.8 KB)
static Tca8418Keyboard keyboard;
static bool radioReady = false;

static DeviceSettings settings;
static DeviceSystem sys(settings);
static DeviceMeshStub mesh(settings, radioReady);
static App app(Services{&mesh, &sys, &settings}, canvas);

static PlaceholderScreen messagesScreen("Messages", "Phase 3 : messagerie");
static PlaceholderScreen contactsScreen("Contacts", "Phase 3 : messagerie");
static PlaceholderScreen gpsScreen("GPS", "Phase 4 : GPS et boussole");
static PlaceholderScreen mapScreen("Carte", "Phase 5 : cartes hors ligne");
static TextInputScreen textInput;
static SystemScreen systemScreen;
static SettingsScreen settingsScreen(textInput, systemScreen);
static HomeScreen home(&messagesScreen, &contactsScreen, &gpsScreen, &mapScreen, &settingsScreen);

static uint32_t lastActivity = 0;
static bool screenAsleep = false;
static uint32_t lastHeapLog = 0;
static uint8_t brightness = 96;

static void wakeScreen() {
  if (!screenAsleep) return;
  M5.Display.wakeup();
  M5.Display.setBrightness(brightness);
  screenAsleep = false;
  app.invalidate();
  Serial.println("[CPADV] screen on");
}

static void sleepScreen() {
  if (screenAsleep) return;
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  screenAsleep = true;
  Serial.println("[CPADV] screen off (radio and GPS keep running)");
}

void setup() {
  Serial.begin(115200);
  delay(300);   // let USB CDC come up so the first logs are not lost
  Serial.println();
  Serial.printf("[CPADV] standalone firmware %s (%s %s)\n", CPADV_VERSION, __DATE__, __TIME__);

  board.begin();

  if (!LittleFS.begin(true)) Serial.println("[CPADV] ERROR: LittleFS mount failed");
  settings.load();

  radioReady = radio_init();
  settings.setRadioReady(radioReady);
  if (radioReady) settings.applyRadio();

  sensors.begin();
  keyboard.begin();

  M5.Display.setRotation(1);
  M5.Display.setBrightness(brightness);
  canvas.setColorDepth(16);
  canvas.setPsram(false);
  if (!canvas.createSprite(theme::SCREEN_W, theme::SCREEN_H)) {
    Serial.println("[CPADV] ERROR: cannot allocate the 64.8 KB framebuffer");
  }

  app.begin(&home);
  lastActivity = millis();
  Serial.printf("[CPADV] ready, free heap %u, min %u, largest block %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                ESP.getMaxAllocHeap());
}

void loop() {
  KeyEvent e;
  while (keyboard.poll(e)) {
    lastActivity = millis();
    if (screenAsleep) {   // first key only wakes the screen
      wakeScreen();
      continue;
    }
    app.handleKey(e);
  }

  sensors.loop();

  if (!screenAsleep && app.loop()) {
    canvas.pushSprite(&M5.Display, 0, 0);
  }

  uint16_t timeout = settings.get().screenTimeoutSecs;
  if (timeout && !screenAsleep && millis() - lastActivity > timeout * 1000UL) sleepScreen();

  if (millis() - lastHeapLog > 60000) {
    lastHeapLog = millis();
    Serial.printf("[CPADV] heap free %u min %u largest %u, keys %u, radio rx %u tx %u\n", ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), keyboard.eventsRead(),
                  radioReady ? radio_driver.getPacketsRecv() : 0, radioReady ? radio_driver.getPacketsSent() : 0);
  }

  delay(5);
}
