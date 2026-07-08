// ==================================================
// ANIMATED EMOJI FACE + WIFI CLOCK
// OLED: SSD1306 128x64, I2C
// ==================================================

#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------- WIFI / TIME CONFIG ----------------
const char* WIFI_SSID     = "xxx";
const char* WIFI_PASSWORD = "xxx";

const char* NTP_SERVER   = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 0;       // set to your timezone offset in seconds
const int   DST_OFFSET_SEC = 0;       // daylight saving offset in seconds

// ---------------- TOUCH INPUT ----------------
const int touchPin = 4;
bool lastTouchState = LOW;
unsigned long lastTouchChange = 0;
const unsigned long DEBOUNCE_MS = 200;

// ---------------- PAGE STATE ----------------
enum Page { PAGE_FACE, PAGE_CLOCK };
Page currentPage = PAGE_FACE;

// ==================================================
// MOOD / EMOTION PARTICLE BITMAPS (16x16)
// ==================================================
const unsigned char bmp_heart[] PROGMEM = { 0x00, 0x00, 0x0c, 0x60, 0x1e, 0xf0, 0x3f, 0xf8, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x3f, 0xf8, 0x1f, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_zzz[]   PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x0c, 0x00, 0x18, 0x00, 0x30, 0x00, 0x7e, 0x00, 0x00, 0x3c, 0x00, 0x0c, 0x00, 0x18, 0x00, 0x30, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_anger[] PROGMEM = { 0x00, 0x00, 0x11, 0x10, 0x2a, 0x90, 0x44, 0x40, 0x80, 0x20, 0x80, 0x20, 0x44, 0x40, 0x2a, 0x90, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#define MOOD_NORMAL     0
#define MOOD_HAPPY      1
#define MOOD_SURPRISED  2
#define MOOD_SLEEPY     3
#define MOOD_ANGRY      4
#define MOOD_SAD        5
#define MOOD_EXCITED    6
#define MOOD_LOVE       7
#define MOOD_SUSPICIOUS 8

int currentMood = MOOD_NORMAL;
unsigned long nextMoodChange = 0;

// Cycles the mood every so often so the face feels alive.
// (No weather input for now — purely time-based randomness.)
void updateMoodRandomly() {
  unsigned long now = millis();
  if (now < nextMoodChange) return;
  nextMoodChange = now + random(6000, 15000);

  int moods[] = { MOOD_NORMAL, MOOD_HAPPY, MOOD_SURPRISED, MOOD_SLEEPY,
                   MOOD_ANGRY, MOOD_SAD, MOOD_EXCITED, MOOD_LOVE, MOOD_SUSPICIOUS };
  currentMood = moods[random(0, 9)];
}

// ==================================================
// EYE STRUCT — this was missing entirely before
// ==================================================
struct Eye {
  float x, y, w, h;                 // current position/size
  float targetX, targetY, targetW, targetH;
  float pupilX, pupilY;             // current pupil offset from eye center
  float targetPupilX, targetPupilY;

  bool blinking = false;
  unsigned long lastBlink = 0;
  unsigned long nextBlinkTime = 0;

  void update() {
    const float lerpSpeed = 0.35f;
    x += (targetX - x) * lerpSpeed;
    y += (targetY - y) * lerpSpeed;
    w += (targetW - w) * lerpSpeed;
    h += (targetH - h) * lerpSpeed;
    pupilX += (targetPupilX - pupilX) * lerpSpeed;
    pupilY += (targetPupilY - pupilY) * lerpSpeed;
  }
};

Eye leftEye;
Eye rightEye;

float breathVal = 0;
unsigned long lastSaccade = 0;
unsigned long saccadeInterval = 1500;

void initEyes() {
  unsigned long now = millis();

  leftEye.x = leftEye.targetX = 18;
  leftEye.y = leftEye.targetY = 14;
  leftEye.w = leftEye.targetW = 36;
  leftEye.h = leftEye.targetH = 36;
  leftEye.nextBlinkTime = now + random(2000, 6000);

  rightEye.x = rightEye.targetX = 74;
  rightEye.y = rightEye.targetY = 14;
  rightEye.w = rightEye.targetW = 36;
  rightEye.h = rightEye.targetH = 36;
}

// ==================================================
// DRAWING & ANIMATION
// ==================================================

void drawEyelidMask(float x, float y, float w, float h, int mood, bool isLeft) {
  int ix = (int)x;
  int iy = (int)y;
  int iw = (int)w;
  int ih = (int)h;

  if (mood == MOOD_ANGRY) {
    if (isLeft)
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SSD1306_BLACK);
    else
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SSD1306_BLACK);
  } else if (mood == MOOD_SAD) {
    if (isLeft)
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SSD1306_BLACK);
    else
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SSD1306_BLACK);
  } else if (mood == MOOD_HAPPY || mood == MOOD_LOVE || mood == MOOD_EXCITED) {
    display.fillRect(ix, iy + ih - 12, iw, 14, SSD1306_BLACK);
    display.fillCircle(ix + iw / 2, iy + ih + 6, iw / 1.3, SSD1306_BLACK);
  } else if (mood == MOOD_SLEEPY) {
    display.fillRect(ix, iy, iw, ih / 2 + 2, SSD1306_BLACK);
  } else if (mood == MOOD_SUSPICIOUS) {
    if (isLeft) display.fillRect(ix, iy, iw, ih / 2 - 2, SSD1306_BLACK);
    else display.fillRect(ix, iy + ih - 8, iw, 8, SSD1306_BLACK);
  }
}

void drawUltraProEye(Eye& e, bool isLeft) {
  int ix = (int)e.x;
  int iy = (int)e.y;
  int iw = (int)e.w;
  int ih = (int)e.h;

  if (iw < 4) iw = 4;
  if (ih < 4) ih = 4;

  int r = (iw < 20) ? 3 : 8;
  display.fillRoundRect(ix, iy, iw, ih, r, SSD1306_WHITE);

  int cx = ix + iw / 2;
  int cy = iy + ih / 2;

  int pw = iw / 2.2;
  int ph = ih / 2.2;

  int px = cx + (int)e.pupilX - (pw / 2);
  int py = cy + (int)e.pupilY - (ph / 2);

  if (px < ix) px = ix;
  if (px + pw > ix + iw) px = ix + iw - pw;
  if (py < iy) py = iy;
  if (py + ph > iy + ih) py = iy + ih - ph;

  display.fillRoundRect(px, py, pw, ph, r / 2, SSD1306_BLACK);

  if (iw > 15 && ih > 15) {
    display.fillCircle(px + pw - 4, py + 4, 2, SSD1306_WHITE);
  }

  drawEyelidMask(e.x, e.y, e.w, e.h, currentMood, isLeft);
}

void updatePhysicsAndMood() {
  unsigned long now = millis();
  breathVal = sin(now / 800.0) * 1.5;

  // --- BLINK LOGIC ---
  if (now > leftEye.nextBlinkTime) {
    leftEye.blinking = true;
    leftEye.lastBlink = now;
    rightEye.blinking = true;
    leftEye.nextBlinkTime = now + random(2000, 6000);
  }
  if (leftEye.blinking) {
    leftEye.targetH = 2;
    rightEye.targetH = 2;
    if (now - leftEye.lastBlink > 120) {
      leftEye.blinking = false;
      rightEye.blinking = false;
    }
  }

  // --- SACCADE (Gaze) LOGIC ---
  if (!leftEye.blinking && now - lastSaccade > saccadeInterval) {
    lastSaccade = now;
    saccadeInterval = random(500, 3000);

    int dir = random(0, 10);
    float lx = 0, ly = 0;

    if (dir < 4)      { lx = 0;  ly = 0; }
    else if (dir == 4) { lx = -6; ly = -4; }
    else if (dir == 5) { lx = 6;  ly = -4; }
    else if (dir == 6) { lx = -6; ly = 4; }
    else if (dir == 7) { lx = 6;  ly = 4; }
    else if (dir == 8) { lx = 8;  ly = 0; }
    else if (dir == 9) { lx = -8; ly = 0; }

    leftEye.targetPupilX = lx;
    leftEye.targetPupilY = ly;
    rightEye.targetPupilX = lx;
    rightEye.targetPupilY = ly;

    leftEye.targetX = 18 + (lx * 0.3);
    leftEye.targetY = 14 + (ly * 0.3);
    rightEye.targetX = 74 + (lx * 0.3);
    rightEye.targetY = 14 + (ly * 0.3);
  }

  // --- MOOD SHAPES (Overrides targets) ---
  if (!leftEye.blinking) {
    float baseW = 36;
    float baseH = 36 + breathVal;

    switch (currentMood) {
      case MOOD_NORMAL:
        leftEye.targetW = baseW; leftEye.targetH = baseH;
        rightEye.targetW = baseW; rightEye.targetH = baseH;
        break;
      case MOOD_HAPPY:
      case MOOD_LOVE:
        leftEye.targetW = 40; leftEye.targetH = 32;
        rightEye.targetW = 40; rightEye.targetH = 32;
        break;
      case MOOD_SURPRISED:
        leftEye.targetW = 30; leftEye.targetH = 45;
        rightEye.targetW = 30; rightEye.targetH = 45;
        leftEye.targetPupilX += random(-1, 2);
        break;
      case MOOD_SLEEPY:
        leftEye.targetW = 38; leftEye.targetH = 30;
        rightEye.targetW = 38; rightEye.targetH = 30;
        break;
      case MOOD_ANGRY:
        leftEye.targetW = 34; leftEye.targetH = 32;
        rightEye.targetW = 34; rightEye.targetH = 32;
        break;
      case MOOD_SAD:
        leftEye.targetW = 34; leftEye.targetH = 40;
        rightEye.targetW = 34; rightEye.targetH = 40;
        break;
      case MOOD_EXCITED:
        leftEye.targetW = 40; leftEye.targetH = 40;
        rightEye.targetW = 40; rightEye.targetH = 40;
        break;
      case MOOD_SUSPICIOUS:
        leftEye.targetW = 36; leftEye.targetH = 20;
        rightEye.targetW = 36; rightEye.targetH = 42;
        break;
    }
  }

  leftEye.update();
  rightEye.update();
}

void drawEmoPage() {
  display.clearDisplay();
  updateMoodRandomly();
  updatePhysicsAndMood();

  if (currentMood == MOOD_LOVE) {
    display.drawBitmap(56, 0, bmp_heart, 16, 16, SSD1306_WHITE);
  } else if (currentMood == MOOD_SLEEPY) {
    display.drawBitmap(110, 0, bmp_zzz, 16, 16, SSD1306_WHITE);
  } else if (currentMood == MOOD_ANGRY) {
    display.drawBitmap(56, 0, bmp_anger, 16, 16, SSD1306_WHITE);
  }

  drawUltraProEye(leftEye, true);
  drawUltraProEye(rightEye, false);
  display.display();
}

// ==================================================
// WIFI CLOCK PAGE
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

// ==================================================
// WIFI SETUP
// ==================================================
void connectWiFi() {
  display.clearDisplay();
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 28);
  display.print("Connecting WiFi...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  }
}

// ==================================================
// SETUP / LOOP
// ==================================================
void setup() {
  pinMode(touchPin, INPUT);
  randomSeed(analogRead(0));

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  initEyes();
  connectWiFi();

  currentPage = PAGE_FACE;
}

void loop() {
  // Edge-detect touch to toggle page (debounced)
  bool touchState = digitalRead(touchPin);
  unsigned long now = millis();

  if (touchState != lastTouchState && now - lastTouchChange > DEBOUNCE_MS) {
    lastTouchChange = now;
    lastTouchState = touchState;
    if (touchState == HIGH) {
      currentPage = (currentPage == PAGE_FACE) ? PAGE_CLOCK : PAGE_FACE;
    }
  }

  if (currentPage == PAGE_FACE) {
    drawEmoPage();
  } else {
    drawClock();
  }

  delay(20); // ~50fps cap, keeps animation smooth without hammering the bus
}
