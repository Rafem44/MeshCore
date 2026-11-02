#include "CardputerKeys.h"
#ifdef DISPLAY_CLASS   // companion firmware only (the standalone firmware has its own UI)
#include <Arduino.h>
#include "../../examples/cardputer_ui/device/Tca8418Keyboard.h"

namespace {
Tca8418Keyboard g_keyboard;
bool g_started = false;
uint8_t g_pending[4] = {};   // clicks waiting, per role

void startKeyboard() {
  if (g_started) return;
  g_started = true;
  g_keyboard.begin();   // M5.begin() already ran in CardputerBoard::begin()
  Serial.println("[CPADV] companion: keyboard drives the screen (, / pages, Enter action)");
}

void pollKeyboard() {
  KeyEvent e;
  while (g_keyboard.poll(e)) {
    int role = -1;
    if (e.code == KeyCode::Right || e.code == KeyCode::Down || e.is('/') || e.is('.')) role = CardputerKeyButton::RIGHT;
    else if (e.code == KeyCode::Left || e.code == KeyCode::Up || e.is(',') || e.is(';')) role = CardputerKeyButton::LEFT;
    else if (e.code == KeyCode::Enter || e.is(' ')) role = CardputerKeyButton::ENTER;
    else if (e.code == KeyCode::Esc || e.is('`')) role = CardputerKeyButton::BACK;
    if (role >= 0 && g_pending[role] < 4) g_pending[role]++;
  }
}
}  // namespace

void CardputerKeyButton::begin() {
  MomentaryButton::begin();   // physical pin, if any (G0 for Enter)
  startKeyboard();
}

int CardputerKeyButton::check(bool repeat_click) {
  startKeyboard();            // the joystick buttons are never begun by the UI
  pollKeyboard();
  if (g_pending[_role]) {
    g_pending[_role]--;
    // "Back" is mapped to the UI's triple-click (select) action
    return _role == BACK ? BUTTON_EVENT_TRIPLE_CLICK : BUTTON_EVENT_CLICK;
  }
  return MomentaryButton::check(repeat_click);
}
#endif  // DISPLAY_CLASS
