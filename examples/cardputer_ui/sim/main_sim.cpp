// Cardputer UI simulator (headless).
//
// Runs the portable UI (examples/cardputer_ui/core) on a PC, with fake services,
// driven by a scenario script. Writes PNG screenshots (scaled x3) on demand.
//
// Usage: cardputer_sim <scenario.txt> [output_dir]      (scenario "-" = stdin)
//
// Scenario commands (one per line; lines starting with '#' are comments):
//   wait <ms>                      advance the virtual clock (UI loop runs every 20 ms)
//   key <k>[*N]                    one key press (N times): a char, or enter|esc|bksp|tab|up|down|left|right|space,
//                                  optional prefixes fn+ shift+ ctrl+ alt+   (e.g. key fn+;  key bksp*5)
//   type <text>                    type each character of <text>
//   rx <from> <text...>            receive a direct message
//   rxch <channel> <from> <text>   receive a channel message (shown as "#channel from: text")
//   hear <name> <snr>              a neighbour advert is heard
//   tca <row> <col> [down|up]      raw Cardputer ADV key through the TCA8418 decoding (KeyMatrix),
//                                  row 0..3 / col 0..13 of the physical keyboard; default = press+release
//   name <self name>               set this node's name (without going through the settings screen)
//   battery <pct> [charging]
//   gps fix <sats> | gps nofix
//   ble on|off|connected
//   time <unix>                    set the wall clock (UTC)
//   shot <name>                    write <output_dir>/<name>.png
//   expect-screen <title>          fail (exit 1) if the current screen title differs
//   expect-unread <n>              fail if the unread counter differs
//   expect-setting <field> <value> name|preset|tx|tz|timeout|sound|ble (tz in minutes, bools 0/1)
#include "../core/App.h"
#include "../core/screens/HomeScreen.h"
#include "../core/screens/SettingsScreens.h"
#include "../core/KeyMatrix.h"
#include "SimServices.h"
#include "PngWriter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

static constexpr int ZOOM = 3;
static constexpr uint32_t LOOP_MS = 20;

static LGFX_Sprite g_canvas;     // the single 240x135 RGB565 framebuffer (same as on the device)
static int g_frames = 0;
static int g_failures = 0;

static bool writePng(const std::string& path) {
  const int W = theme::SCREEN_W * ZOOM, H = theme::SCREEN_H * ZOOM;
  std::vector<uint8_t> rgb((size_t)W * H * 3);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      uint16_t c = g_canvas.readPixel(x / ZOOM, y / ZOOM);   // RGB565
      uint8_t* p = &rgb[((size_t)y * W + x) * 3];
      p[0] = ((c >> 11) & 0x1F) * 255 / 31;
      p[1] = ((c >> 5) & 0x3F) * 255 / 63;
      p[2] = (c & 0x1F) * 255 / 31;
    }
  }
  return png::write(path, W, H, rgb);
}

static bool parseKey(std::string k, KeyEvent& e) {
  e = KeyEvent();
  for (;;) {
    if (k.rfind("fn+", 0) == 0 && k.size() > 3) { e.mods |= MOD_FN; k = k.substr(3); }
    else if (k.rfind("shift+", 0) == 0 && k.size() > 6) { e.mods |= MOD_SHIFT; k = k.substr(6); }
    else if (k.rfind("ctrl+", 0) == 0 && k.size() > 5) { e.mods |= MOD_CTRL; k = k.substr(5); }
    else if (k.rfind("alt+", 0) == 0 && k.size() > 4) { e.mods |= MOD_ALT; k = k.substr(4); }
    else break;
  }
  if (k == "enter") e.code = KeyCode::Enter;
  else if (k == "esc") e.code = KeyCode::Esc;
  else if (k == "bksp") e.code = KeyCode::Backspace;
  else if (k == "tab") e.code = KeyCode::Tab;
  else if (k == "up") e.code = KeyCode::Up;
  else if (k == "down") e.code = KeyCode::Down;
  else if (k == "left") e.code = KeyCode::Left;
  else if (k == "right") e.code = KeyCode::Right;
  else if (k == "space") { e.code = KeyCode::Char; e.ch = ' '; }
  else if (k.size() == 1) {
    e.code = KeyCode::Char;
    e.ch = k[0];
    // Same translation as the hardware driver: Fn + arrow-printed keys = arrows, Fn + ` = Esc
    if (e.mods & MOD_FN) {
      KeyEvent n = toNavigation(e);
      n.mods = e.mods;
      e = n;
    } else if ((e.mods & MOD_SHIFT) && e.ch >= 'a' && e.ch <= 'z') {
      e.ch -= 32;
    }
  } else {
    return false;
  }
  return true;
}

static std::string rest(std::istringstream& in) {
  std::string r;
  std::getline(in, r);
  size_t p = r.find_first_not_of(' ');
  return p == std::string::npos ? "" : r.substr(p);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <scenario.txt|-> [output_dir]\n", argv[0]);
    return 2;
  }
  std::string outDir = argc > 2 ? argv[2] : ".";

  g_canvas.setColorDepth(16);
  if (!g_canvas.createSprite(theme::SCREEN_W, theme::SCREEN_H)) {
    fprintf(stderr, "cannot allocate framebuffer\n");
    return 2;
  }

  SimSystem sys;
  SimSettings settings;
  strcpy(settings.s.nodeName, "Cardputer-Sim");
  SimMesh mesh(sys, settings);
  Services svc{&mesh, &sys, &settings};
  App app(svc, g_canvas);
  KeyMatrix matrix;

  PlaceholderScreen messages("Messages", "Phase 3 : messagerie");
  PlaceholderScreen contacts("Contacts", "Phase 3 : messagerie");
  PlaceholderScreen gps("GPS", "Phase 4 : GPS et boussole");
  PlaceholderScreen map("Carte", "Phase 5 : cartes hors ligne");
  TextInputScreen textInput;
  SystemScreen systemScreen;
  SettingsScreen settingsScreen(textInput, systemScreen);
  HomeScreen home(&messages, &contacts, &gps, &map, &settingsScreen);
  app.begin(&home);

  auto runFor = [&](uint32_t ms) {
    uint32_t end = sys.virtualMillis + ms;
    do {
      if (app.loop()) g_frames++;
      if (sys.virtualMillis >= end) break;
      sys.virtualMillis += LOOP_MS;
    } while (true);
  };

  std::ifstream file;
  std::istream* in = &std::cin;
  if (strcmp(argv[1], "-") != 0) {
    file.open(argv[1]);
    if (!file) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    in = &file;
  }

  std::string line;
  int lineNo = 0;
  runFor(0);
  while (std::getline(*in, line)) {
    lineNo++;
    size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') continue;   // blank line or comment
    std::istringstream ls(line);
    std::string cmd;
    if (!(ls >> cmd)) continue;

    if (cmd == "wait") {
      uint32_t ms = 0; ls >> ms; runFor(ms);
    } else if (cmd == "key") {
      std::string k; ls >> k;
      int times = 1;
      size_t star = k.rfind('*');
      if (star != std::string::npos && star > 0 && star + 1 < k.size()) { times = atoi(k.c_str() + star + 1); k = k.substr(0, star); }
      KeyEvent e;
      if (!parseKey(k, e)) { fprintf(stderr, "line %d: unknown key '%s'\n", lineNo, k.c_str()); g_failures++; continue; }
      for (int i = 0; i < times; i++) { app.handleKey(e); runFor(LOOP_MS); }
    } else if (cmd == "type") {
      std::string t = rest(ls);
      for (char c : t) { KeyEvent e; e.code = KeyCode::Char; e.ch = c; app.handleKey(e); runFor(LOOP_MS); }
    } else if (cmd == "rx") {
      std::string from; ls >> from; mesh.receive(from, from, rest(ls), false); runFor(LOOP_MS);
    } else if (cmd == "rxch") {
      std::string ch, from; ls >> ch >> from; mesh.receive(from, ch, rest(ls), true); runFor(LOOP_MS);
    } else if (cmd == "hear") {
      std::string n; float snr = 0; ls >> n >> snr; mesh.hear(n, snr); runFor(LOOP_MS);
    } else if (cmd == "tca") {
      int r = -1, c = -1; std::string mode; ls >> r >> c >> mode;
      uint8_t k = KeyMatrix::fromGrid(r, c);
      if (!k) { fprintf(stderr, "line %d: no key at %d,%d\n", lineNo, r, c); g_failures++; continue; }
      KeyEvent e;
      if (mode != "up" && matrix.feed(0x80 | k, e)) app.handleKey(e);
      if (mode != "down" && matrix.feed(k, e)) app.handleKey(e);
      runFor(LOOP_MS);
    } else if (cmd == "name") {
      UiSettings s = settings.get(); std::string n = rest(ls);
      strncpy(s.nodeName, n.c_str(), sizeof(s.nodeName) - 1); s.nodeName[sizeof(s.nodeName) - 1] = 0;
      settings.save(s); app.invalidate(); runFor(LOOP_MS);
    } else if (cmd == "battery") {
      std::string c; ls >> sys.st.batteryPercent >> c; sys.st.charging = (c == "charging"); app.invalidate(); runFor(LOOP_MS);
    } else if (cmd == "gps") {
      std::string m; ls >> m;
      sys.st.gpsFix = (m == "fix");
      if (sys.st.gpsFix) ls >> sys.st.gpsSatellites; else sys.st.gpsSatellites = 0;
      app.invalidate(); runFor(LOOP_MS);
    } else if (cmd == "ble") {
      std::string m; ls >> m;
      sys.st.bleEnabled = (m != "off"); sys.st.bleConnected = (m == "connected");
      app.invalidate(); runFor(LOOP_MS);
    } else if (cmd == "time") {
      uint32_t t; ls >> t; sys.st.unixTime = t - sys.virtualMillis / 1000; app.invalidate(); runFor(LOOP_MS);
    } else if (cmd == "shot") {
      std::string n; ls >> n;
      app.drawAll();
      std::string path = outDir + "/" + n + ".png";
      if (writePng(path)) printf("[SIM] screenshot %s\n", path.c_str());
      else { fprintf(stderr, "line %d: cannot write %s\n", lineNo, path.c_str()); g_failures++; }
    } else if (cmd == "expect-screen") {
      std::string want = rest(ls);
      const char* got = app.top() ? app.top()->title() : "";
      if (want != got) { fprintf(stderr, "line %d: FAIL expected screen '%s', got '%s'\n", lineNo, want.c_str(), got); g_failures++; }
    } else if (cmd == "expect-unread") {
      int want = 0; ls >> want;
      int got = mesh.unreadCount();
      if (want != got) { fprintf(stderr, "line %d: FAIL expected %d unread, got %d\n", lineNo, want, got); g_failures++; }
    } else if (cmd == "expect-setting") {
      std::string field; ls >> field; std::string want = rest(ls);
      const UiSettings& s = settings.get();
      std::string got;
      if (field == "name") got = s.nodeName;
      else if (field == "preset") got = std::to_string(s.radioPreset);
      else if (field == "tx") got = std::to_string(s.txPowerDbm);
      else if (field == "tz") got = std::to_string(s.utcOffsetMinutes);
      else if (field == "timeout") got = std::to_string(s.screenTimeoutSecs);
      else if (field == "sound") got = std::to_string(s.sound);
      else if (field == "ble") got = std::to_string(s.ble);
      else got = "<unknown field>";
      if (want != got) { fprintf(stderr, "line %d: FAIL setting %s expected '%s', got '%s'\n", lineNo, field.c_str(), want.c_str(), got.c_str()); g_failures++; }
    } else {
      fprintf(stderr, "line %d: unknown command '%s'\n", lineNo, cmd.c_str());
      g_failures++;
    }
  }

  printf("[SIM] done: %u ms simulated, %d frames drawn, %d failure(s)\n", sys.virtualMillis, g_frames, g_failures);
  return g_failures ? 1 : 0;
}
