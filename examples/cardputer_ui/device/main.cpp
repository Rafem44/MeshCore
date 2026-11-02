// Standalone Cardputer ADV firmware: keyboard messaging UI on top of MeshCore.
//
// Build variants (variants/m5stack_cardputer_adv/platformio.ini):
//   cardputer_adv_standalone       messaging, contacts, channels, GPS, compass, backup
//   cardputer_adv_standalone_map   + offline map from the SD card            (CPADV_MAP=1)
//   cardputer_adv_demo             no LoRa Cap needed: simulated network and GPS (CPADV_DEMO=1)
#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <helpers/ArduinoHelpers.h>
#include <target.h>

#include "../core/App.h"
#include "../core/screens/GpsScreens.h"
#include "../core/screens/HomeScreen.h"
#include "../core/screens/MessagingScreens.h"
#include "../core/screens/SettingsScreens.h"
#include "DeviceBackup.h"
#include "DeviceServices.h"
#include "LittleFsStorage.h"
#include "SdCard.h"
#include "SdTiles.h"
#include "Tca8418Keyboard.h"
#if CPADV_MAP
  #include "../core/screens/MapScreen.h"
#endif
#if CPADV_DEMO
  #include "../core/DemoGps.h"
  #include "../core/DemoMesh.h"
#else
  #include "CardputerMesh.h"
  #include "DeviceGps.h"
#endif

static M5Canvas canvas(&M5.Display);   // the single 240x135 RGB565 framebuffer (64.8 KB)
static Tca8418Keyboard keyboard;
static bool radioReady = false;
static DeviceSettings settings;
static DeviceBackup backup;

// ---------------------------------------------------------------- services
#if CPADV_DEMO
// The demo GPS needs the system clock and the system needs the GPS: a proxy breaks the cycle.
class GpsProxy : public GpsService {
public:
  GpsService* target = nullptr;
  const GpsState& state() override { static GpsState none; return target ? target->state() : none; }
  bool present() override { return target && target->present(); }
  uint32_t baud() override { return target ? target->baud() : 0; }
  void coldRestart() override { if (target) target->coldRestart(); }
};
static GpsProxy gpsProxy;
static LittleFsStorage demoStorage;
static DeviceSystem sys(settings, gpsProxy);
static DemoGps gpsDev(sys);
static DemoMesh meshNode(sys, settings, demoStorage);
#else
static DeviceGps gpsDev;
static DeviceSystem sys(settings, gpsDev);
static StdRNG fastRng;
static SimpleMeshTables tables;
static CardputerMesh meshNode(radio_driver, fastRng, rtc_clock, tables, settings);
#endif

#if CPADV_MAP
static SdTiles sdTiles;
static MapScreen mapScreen;
static TileSource* const tileSource = &sdTiles;
#else
static TileSource* const tileSource = nullptr;
#endif

static App app(Services{&meshNode, &sys, &settings, &gpsDev, tileSource, &backup}, canvas);

// ---------------------------------------------------------------- screens
static TextInputScreen textInput;
static ChatScreen chatScreen;
static ContactDetailScreen contactDetail(chatScreen);
static ContactsScreen contactsScreen(chatScreen, contactDetail);
static ConversationsScreen conversations(chatScreen, contactsScreen, textInput);
static CompassScreen compassScreen;
static GpsScreen gpsScreen(compassScreen);
static SystemScreen systemScreen;
static SettingsScreen settingsScreen(textInput, systemScreen);
#if CPADV_MAP
static HomeScreen home(&conversations, &contactsScreen, &gpsScreen, &mapScreen, &settingsScreen);
#else
static HomeScreen home(&conversations, &contactsScreen, &gpsScreen, nullptr, &settingsScreen);   // no map built in
#endif

// ---------------------------------------------------------------- power / screen
static uint32_t lastActivity = 0;
static bool screenAsleep = false;
static uint32_t lastHeapLog = 0;
static uint32_t lastBatteryCheck = 0;
static bool lowBatteryWarned = false;
static uint32_t bootAdvertAt = 0;
static uint8_t appliedBrightness = 0;

static uint8_t brightnessLevel() {
  static const uint8_t LEVELS[] = {40, 110, 200};
  uint8_t b = settings.get().brightness;
  return LEVELS[b < 3 ? b : 1];
}

static void meshLoop() {
#if CPADV_DEMO
  gpsDev.tick();
  meshNode.tick();
#else
  meshNode.loop();
#endif
}

static void wakeScreen() {
  if (!screenAsleep) return;
  setCpuFrequencyMhz(240);   // full speed for drawing
  M5.Display.wakeup();
  M5.Display.setBrightness(brightnessLevel());
  screenAsleep = false;
  app.invalidate();
  Serial.println("[CPADV] screen on");
}

static void sleepScreen() {
  if (screenAsleep) return;
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  screenAsleep = true;
  setCpuFrequencyMhz(80);    // radio, GPS and keyboard only: 80 MHz is plenty and saves battery
  Serial.println("[CPADV] screen off, CPU 80 MHz (radio and GPS keep running)");
}

#if CPADV_DEMO
// Build date as the demo clock (no GPS / radio to set it)
static uint32_t buildUnix() {
  static const char* const M = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], 0};
  int mo = (int)(strstr(M, mon) - M) / 3 + 1, d = atoi(__DATE__ + 4), y = atoi(__DATE__ + 7);
  int hh = atoi(__TIME__), mi = atoi(__TIME__ + 3), ss = atoi(__TIME__ + 6);
  y -= mo <= 2;
  int era = y / 400, yoe = y - era * 400;
  int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  uint32_t days = era * 146097 + doe - 719468;
  return days * 86400u + hh * 3600 + mi * 60 + ss;   // build machine time (CI: UTC)
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);   // let USB CDC come up so the first logs are not lost
  Serial.println();
  Serial.printf("[CPADV] firmware %s (%s %s)\n", CPADV_VERSION, __DATE__, __TIME__);

  board.begin();

  if (!LittleFS.begin(true)) Serial.println("[CPADV] ERROR: LittleFS mount failed");
  Serial.printf("[CPADV] LittleFS %u / %u KB used\n", (unsigned)(LittleFS.usedBytes() / 1024),
                (unsigned)(LittleFS.totalBytes() / 1024));
  settings.load();
  {
    // ~16 KB of flash per conversation history (64 messages + file system overhead)
    int total = (int)(LittleFS.totalBytes() / 1024);
    int convs = (total - 96) / 16;
    meshNode.messages().setCapacity(convs);
    Serial.printf("[CPADV] storage %d KB -> up to %d conversations kept\n", total,
                  meshNode.messages().capacity());
  }

#if CPADV_DEMO
  gpsProxy.target = &gpsDev;
  uint32_t t = buildUnix();
  if (rtc_clock.getCurrentTime() < t) rtc_clock.setCurrentTime(t);
  gpsDev.baseUnix = t;
  DemoMesh::logSink = [](const char* line) { Serial.printf("[CPADV] demo: %s\n", line); };
  meshNode.begin();
  meshNode.gps = &gpsDev;
  meshNode.enableBot(esp_random());
  meshNode.seedDemo();
  Serial.println("[CPADV] DEMO mode: no radio, simulated contacts and GPS");
#else
  radioReady = radio_init();
  settings.setRadioReady(radioReady);
  if (radioReady) {
    fastRng.begin(radio_driver.getRngSeed());
    meshNode.begin();
    settings.applyRadio();
    bootAdvertAt = millis() + 3000;   // let the radio settle, then announce ourselves
  }
  meshNode.setGps(&gpsDev);
  gpsDev.begin();   // the GPS UART is ours: EnvironmentSensorManager is not started
  backup.setFlushHook([] { meshNode.flush(); });
#endif

  sdcard::begin();   // after the radio: the card shares its SPI bus
#if CPADV_MAP
  sdTiles.begin();
#endif

  compassScreen.setChat(&chatScreen);
  contactDetail.setCompass(&compassScreen);
  keyboard.begin();

  M5.Display.setRotation(1);
  appliedBrightness = brightnessLevel();
  M5.Display.setBrightness(appliedBrightness);
  canvas.setColorDepth(16);
  canvas.setPsram(false);
  if (!canvas.createSprite(theme::SCREEN_W, theme::SCREEN_H)) {
    Serial.println("[CPADV] ERROR: cannot allocate the 64.8 KB framebuffer");
  }

  sys.setYieldHook(meshLoop);   // long renders (map) keep the radio served
  app.begin(&home);
#if CPADV_DEMO
  app.notify("Mode démo", "Pas de radio : contacts et GPS simulés", 5000);
#else
  if (!radioReady) app.notify("Radio absente", "Vérifier le Cap LoRa et l'antenne", 8000);
#endif
  lastActivity = millis();
  Serial.printf("[CPADV] ready, free heap %u, min %u, largest block %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                ESP.getMaxAllocHeap());
}

void loop() {
  // Radio first: never let the UI delay packet handling
  meshLoop();

  KeyEvent e;
  while (keyboard.poll(e)) {
    lastActivity = millis();
    if (screenAsleep) {   // first key only wakes the screen
      wakeScreen();
      continue;
    }
    app.handleKey(e);
    meshLoop();
  }

#if !CPADV_DEMO
  gpsDev.loop();
  if (bootAdvertAt && (int32_t)(millis() - bootAdvertAt) >= 0) {
    bootAdvertAt = 0;
    meshNode.sendAdvert(true, settings.get().shareLocation);
  }
#endif

  if (screenAsleep) {
    // Screen off: no drawing at all, but events still notify (sound) and wake the screen
    if (app.pollEvents() && app.bannerVisible()) {
      lastActivity = millis();
      wakeScreen();
    }
  } else {
    if (brightnessLevel() != appliedBrightness) {   // changed in the settings
      appliedBrightness = brightnessLevel();
      M5.Display.setBrightness(appliedBrightness);
    }
    if (app.loop()) canvas.pushSprite(&M5.Display, 0, 0);
  }

  uint16_t timeout = settings.get().screenTimeoutSecs;
  if (timeout && !screenAsleep && millis() - lastActivity > timeout * 1000UL) sleepScreen();

  // Battery: one warning at 10 %, and the level in the logs for autonomy measurements
  if (millis() - lastBatteryCheck > 30000) {
    lastBatteryCheck = millis();
    int pct = M5.Power.getBatteryLevel();
    if (pct >= 0 && pct <= 10 && !lowBatteryWarned) {
      lowBatteryWarned = true;
      char t[40];
      snprintf(t, sizeof(t), "%d %%, pense à recharger", pct);
      wakeScreen();
      app.notify("Batterie faible", t, 5000);
      sys.playSound(SOUND_FAIL);
    } else if (pct > 20) {
      lowBatteryWarned = false;
    }
  }

  if (backup.restartPending() && (int32_t)(millis() - backup.restartAt()) >= 0) {
    Serial.println("[CPADV] restarting after restore");
    Serial.flush();
    ESP.restart();
  }

  if (millis() - lastHeapLog > 60000) {
    lastHeapLog = millis();
    RadioStats rs = meshNode.radioStats();
    Serial.printf("[CPADV] heap %u min %u big %u | bat %d%% %umV | screen %s | rx %u tx %u err %u | contacts %d convs %d\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), M5.Power.getBatteryLevel(),
                  board.getBattMilliVolts(), screenAsleep ? "off" : "on", rs.rxPackets, rs.txPackets, rs.rxErrors,
                  meshNode.contactCount(), meshNode.messages().count());
  }

  delay(screenAsleep ? 10 : 2);
}
