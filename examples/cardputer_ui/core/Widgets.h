#pragma once
// Small drawing helpers shared by the screens.
#include "Theme.h"
#include <stddef.h>

// Selection + scrolling state of a vertical list.
struct ListState {
  int sel = 0;
  int top = 0;
  // Moves the selection by `delta` (wraps around) and keeps it visible.
  void move(int delta, int count, int visible);
  void clamp(int count, int visible);
};

constexpr int LIST_VISIBLE_ROWS = theme::CONTENT_H / theme::LINE_H;   // 8

// One list row: label on the left, value right-aligned. y = top of the row.
void drawListRow(lgfx::LovyanGFX& g, int y, const char* label, const char* value,
                 bool selected, uint16_t valueColor = theme::ACCENT, bool dimLabel = false);
void drawScrollbar(lgfx::LovyanGFX& g, int top, int visible, int count);

// UTF-8 string helpers (buffers are NUL-terminated, `cap` includes the terminator)
size_t utf8Len(const char* s);                          // number of code points
void utf8PopBack(char* s);                              // removes the last code point
bool utf8Append(char* s, size_t cap, const char* add);  // false if it does not fit

// French accent composition: Alt + base letter appends the first accented form (Alt+e: é).
// Pressing it again right away cycles through the variants (é -> è -> ê -> ë -> é).
// Returns false if `base` has no accented forms.
bool composeAccent(char* s, size_t cap, char base);
