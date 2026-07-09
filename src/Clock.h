#ifndef CLOCK_H
#define CLOCK_H

// Time (NTP clock) page. Relies on `display`, SCREEN_WIDTH, and WiFi/time.h
// already being available — include this from main.cpp AFTER those are set up.

// ==================================================
// TIME PAGE
// ==================================================
void drawClock() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (WiFi.status() != WL_CONNECTED) {
    display.setFont(NULL);
    display.setCursor(20, 28);
    display.print("WiFi not connected");
    display.display();
    return;
  }

  struct tm t;
  if (!getLocalTime(&t)) {
    display.setFont(NULL);
    display.setCursor(20, 28);
    display.print("Syncing time...");
    display.display();
    return;
  }

  String ampm = (t.tm_hour >= 12) ? "PM" : "AM";
  int h12 = t.tm_hour % 12;
  if (h12 == 0) h12 = 12;

  display.setFont(NULL);
  display.setTextSize(1);
  display.setCursor(112, 0);
  display.print(ampm);

  display.setFont(&FreeSansBold18pt7b);
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", h12, t.tm_min);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 42);
  display.print(timeStr);

  display.setFont(&FreeSans9pt7b);
  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a, %b %d", &t);
  display.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 62);
  display.print(dateStr);

  display.display();
}


#endif