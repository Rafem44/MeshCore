#pragma once
// Cardputer keyboard as the companion UI's buttons (examples/companion_radio/ui-new).
// The stock UI only knows physical buttons (MomentaryButton, check() is virtual): each
// CardputerKeyButton reports clicks from the keyboard, so the screen is driven with
//   , or ;  previous page        / or .  next page        Enter  action ("press Enter")
// The side G0 button keeps working as Enter.
#include <helpers/ui/MomentaryButton.h>

class CardputerKeyButton : public MomentaryButton {
public:
  enum Role : uint8_t { ENTER, LEFT, RIGHT, BACK };
  explicit CardputerKeyButton(Role role, int8_t pin = -1) : MomentaryButton(pin, 1000, true), _role(role) {}
  void begin() override;
  int check(bool repeat_click = false) override;

private:
  Role _role;
};
