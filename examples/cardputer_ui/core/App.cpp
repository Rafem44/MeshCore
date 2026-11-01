#include "App.h"
#include <stdio.h>

using namespace theme;

void App::begin(Screen* root) {
  _depth = 0;
  push(root);
}

void App::push(Screen* s) {
  if (!s || _depth >= MAX_DEPTH) return;
  _stack[_depth++] = s;
  s->onEnter(*this);
  _dirty = true;
}

void App::pop() {
  if (_depth <= 1) return;  // never pop the root screen
  _depth--;
  top()->onEnter(*this);
  _dirty = true;
}

void App::handleKey(const KeyEvent& raw) {
  Screen* s = top();
  if (!s) return;
  KeyEvent e = s->wantsText() ? raw : toNavigation(raw);
  bool used = s->onKey(*this, e);
  if (!used && e.code == KeyCode::Esc) pop();
  _dirty = true;
}

bool App::loop() {
  Screen* s = top();
  if (s && s->tick(*this)) _dirty = true;
  // Status bar shows a clock: refresh at least once per second.
  if (now() - _lastStatusDraw >= 1000) _dirty = true;
  if (!_dirty) return false;
  drawAll();
  return true;
}

void App::drawAll() {
  _dirty = false;
  _lastStatusDraw = now();
  _g.startWrite();
  _g.fillScreen(BG);
  if (Screen* s = top()) {
    _g.setClipRect(0, CONTENT_Y, SCREEN_W, CONTENT_H);
    s->draw(*this, _g);
    _g.clearClipRect();
  }
  drawStatusBar();
  drawHelpLine();
  _g.endWrite();
}

void App::drawStatusBar() {
  DeviceStatus st = _svc.sys->status();
  _g.fillRect(0, 0, SCREEN_W, STATUS_H, BAR_BG);
  _g.setFont(&fontSmall);
  _g.setTextDatum(lgfx::top_left);

  // Left: local time (from GPS / phone), or --:--
  char buf[24];
  if (st.unixTime) {
    uint32_t t = st.unixTime + _svc.settings->get().utcOffsetMinutes * 60;
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)((t / 3600) % 24), (unsigned)((t / 60) % 60));
  } else {
    snprintf(buf, sizeof(buf), "--:--");
  }
  _g.setTextColor(FG);
  _g.drawString(buf, 2, 1);

  // Then the current screen title
  if (Screen* s = top()) {
    _g.setTextColor(ACCENT);
    _g.drawString(s->title(), 36, 1);
  }

  // Right side, drawn right-to-left: battery, BLE, GPS, unread
  int x = SCREEN_W - 2;
  _g.setTextDatum(lgfx::top_right);

  if (st.batteryPercent >= 0) snprintf(buf, sizeof(buf), "%d%%%s", st.batteryPercent, st.charging ? "+" : "");
  else snprintf(buf, sizeof(buf), "?%%");
  _g.setTextColor(st.batteryPercent >= 0 && st.batteryPercent < 15 ? COLOR_BAD : FG);
  _g.drawString(buf, x, 1);
  x -= _g.textWidth(buf) + 6;

  if (st.bleEnabled) {
    _g.setTextColor(st.bleConnected ? ACCENT : DIM);
    _g.drawString("BLE", x, 1);
    x -= _g.textWidth("BLE") + 6;
  }

  if (st.gpsFix) snprintf(buf, sizeof(buf), "GPS%d", st.gpsSatellites);
  else snprintf(buf, sizeof(buf), "GPS-");
  _g.setTextColor(st.gpsFix ? COLOR_OK : DIM);
  _g.drawString(buf, x, 1);
  x -= _g.textWidth(buf) + 6;

  int unread = _svc.mesh->unreadCount();
  if (unread > 0) {
    snprintf(buf, sizeof(buf), "*%d", unread);
    _g.setTextColor(COLOR_WARN);
    _g.drawString(buf, x, 1);
  }
  _g.setTextDatum(lgfx::top_left);
}

void App::drawHelpLine() {
  const int y = SCREEN_H - HELP_H;
  _g.fillRect(0, y, SCREEN_W, HELP_H, BAR_BG);
  if (Screen* s = top()) {
    _g.setFont(&fontSmall);
    _g.setTextColor(DIM);
    char line[96];
    fitText(_g, s->help(), SCREEN_W - 4, line, sizeof(line));
    _g.drawString(line, 2, y + 1);
  }
}
