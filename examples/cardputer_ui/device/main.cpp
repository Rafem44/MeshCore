// Standalone Cardputer ADV firmware: keyboard messaging UI on top of MeshCore.
// Phase 3: conversations, chat with delivery tracking, contacts, channels, notifications.
#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <helpers/ArduinoHelpers.h>
#include <target.h>

#include "../core/App.h"
#include "../core/screens/HomeScreen.h"
#include "../core/screens/MessagingScreens.h"
#include "../core/screens/SettingsScreens.h"
#include "CardputerMesh.h"
#include "DeviceServices.h"
#include "Tca8418Keyboard.h"

static M5Canvas canvas(&M5.Display);   // the single 240x135 RGB565 framebuffer (64.8 KB)
static Tca8418Keyboard keyboard;
static bool radioReady = false;

static DeviceSettings settings;
static DeviceSystem sys(settings);
static StdRNG fastRng;
static SimpleMeshTables tables;
static CardputerMesh meshNode(radio_driver, fastRng, rtc_clock, tables, settings);
static App app(Services{&meshNode, &sys, &settings}, canvas);

static TextInputScreen textInput;
static ChatScreen chatScreen;
static ContactDetailScreen contactDetail(chatScreen);
static ContactsScreen contactsScreen(chatScreen, contactDetail);
static ConversationsScreen conversations(chatScreen, contactsScreen, textInput);
static PlaceholderScreen gpsScreen("GPS", "Phase 4 : GPS et boussole");
static PlaceholderScreen mapScreen("Carte", "Phase 5 : cartes hors ligne");
static SystemScreen systemScreen;
static SettingsScreen settingsScreen(textInput, systemScreen);
static HomeScreen home(&conversations, &contactsScreen, &gpsScreen, &mapScreen, &settingsScreen);

static uint32_t lastActivity = 0;
static bool screenAsleep = false;
static uint32_t lastHeapLog = 0;
static uint32_t bootAdvertAt = 0;
static const uint8_t brightness = 96;

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
  Serial.printf("[CPADV] LittleFS %u / %u KB used\n", (unsigned)(LittleFS.usedBytes() / 1024),
                (unsigned)(LittleFS.totalBytes() / 1024));
  settings.load();

  radioReady = radio_init();
  settings.setRadioReady(radioReady);
  if (radioReady) {
    fastRng.begin(radio_driver.getRngSeed());
    meshNode.begin();
    settings.applyRadio();
    bootAdvertAt = millis() + 3000;   // let the radio settle, then announce ourselves
  }

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
  if (!radioReady) app.notify("Radio absente", "Vérifier le Cap LoRa et l'antenne", 8000);
  lastActivity = millis();
  Serial.printf("[CPADV] ready, free heap %u, min %u, largest block %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                ESP.getMaxAllocHeap());
}

void loop() {
  // Radio first: never let the UI delay packet handling
  meshNode.loop();

  KeyEvent e;
  while (keyboard.poll(e)) {
    lastActivity = millis();
    if (screenAsleep) {   // first key only wakes the screen
      wakeScreen();
      continue;
    }
    app.handleKey(e);
    meshNode.loop();
  }

  sensors.loop();

  if (bootAdvertAt && (int32_t)(millis() - bootAdvertAt) >= 0) {
    bootAdvertAt = 0;
    meshNode.sendAdvert(true);
  }

  if (screenAsleep) {
    // Screen off: no drawing at all, but events still notify (sound) and wake the screen
    if (app.pollEvents() && app.bannerVisible()) {
      lastActivity = millis();
      wakeScreen();
    }
  } else if (app.loop()) {
    canvas.pushSprite(&M5.Display, 0, 0);
  }

  uint16_t timeout = settings.get().screenTimeoutSecs;
  if (timeout && !screenAsleep && millis() - lastActivity > timeout * 1000UL) sleepScreen();

  if (millis() - lastHeapLog > 60000) {
    lastHeapLog = millis();
    RadioStats rs = meshNode.radioStats();
    Serial.printf("[CPADV] heap free %u min %u largest %u | keys %u | radio rx %u tx %u err %u | contacts %d convs %d\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), keyboard.eventsRead(),
                  rs.rxPackets, rs.txPackets, rs.rxErrors, meshNode.contactCount(), meshNode.messages().count());
  }

  delay(2);
}
