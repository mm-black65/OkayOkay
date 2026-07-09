#ifndef SPLASH_H
#define SPLASH_H

// Boot splash screen. Relies on `display`, SCREEN_WIDTH, and the
// FreeSansBold18pt7b / FreeSans9pt7b fonts already being available —
// include this from main.cpp AFTER those are set up.

// ==================================================
// BOOT SPLASH SCREEN
// ==================================================

// Just draws the greeting — no display.display() or delay here,
// so it can be reused by both the blocking boot version and the
// non-blocking in-loop page version below.
void drawSplashContent() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setFont(&FreeSansBold18pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  const char* line1 = "Hi!!";
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 30);
  display.print(line1);

  display.setFont(&FreeSans9pt7b);
  const char* line2 = "I'm OkayOkay";
  display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 52);
  display.print(line2);
}

// Used once at boot — blocks briefly, which is fine since nothing
// else needs to run yet.
void drawSplashScreen() {
  drawSplashContent();
  display.display();
  delay(2200);
}

// Used when the splash reappears as part of the page loop (after the
// Emoji page). Non-blocking: draws once per frame and the main loop
// checks splashLoopEnterTime to auto-advance to the Time page.
void drawSplashPage() {
  drawSplashContent();
  display.display();
}

#endif