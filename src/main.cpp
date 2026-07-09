// ==================================================
// ANIMATED EMOJI FACE + WIFI CLOCK + WEATHER/AQI
// OLED: SSD1306 128x64, I2C
// Buzzer: passive, GPIO 25
// ==================================================

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "Eye.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#include "Splash.h"
#include "Clock.h"

// ---------------- WIFI / TIME CONFIG ----------------
const char* WIFI_SSID     = "xxx";
const char* WIFI_PASSWORD = "xxx";

const char* NTP_SERVER     = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 19800;   // IST = UTC+5:30
const int   DST_OFFSET_SEC = 0;       // India doesn't use DST

// ---------------- BUZZER ----------------
const int BUZZER_PIN = 25;

// ---------------- TOUCH INPUT ----------------
const int touchPin = 4;

bool touchActive = false;
unsigned long touchStartTime = 0;
bool longPressTriggered = false;

const unsigned long HOLD_CUTE_MS = 3000;
const unsigned long HOLD_SIDE_MS = 5000;

bool holdActive = false;
int holdMood = 0;

// ---------------- PAGE STATE ----------------
enum Page { PAGE_SPLASH, PAGE_TIME, PAGE_WEATHER, PAGE_GAME_MENU, PAGE_GAME, PAGE_FACE };
Page currentPage = PAGE_SPLASH;

unsigned long lastActivityTime = 0;              // resets on every touch event
const unsigned long IDLE_TO_FACE_MS = 10000;      // 10s idle on Time/Weather/GameMenu -> Emoji
unsigned long splashLoopEnterTime = 0;            // when we (re)entered the in-loop splash page
const unsigned long SPLASH_LOOP_DURATION_MS = 2000;

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
#define MOOD_SUSPICIOUS 8   // used as the ">5 sec hold" side-eye
#define MOOD_CUTE       9   // used as the ">3 sec hold" cute-eyes

int currentMood = MOOD_NORMAL;
unsigned long nextMoodChange = 0;

// Cycles the mood every so often so the face feels alive when idle
// (random moods only run when the touch isn't being held).
void updateMoodRandomly() {
  unsigned long now = millis();
  if (now < nextMoodChange) return;
  nextMoodChange = now + random(6000, 15000);

  int moods[] = { MOOD_NORMAL, MOOD_HAPPY, MOOD_SURPRISED, MOOD_SLEEPY,
                   MOOD_ANGRY, MOOD_SAD, MOOD_EXCITED, MOOD_LOVE, MOOD_SUSPICIOUS };
  currentMood = moods[random(0, 9)];
}

// ==================================================
// EYE INSTANCES (Eye struct lives in Eye.h)
// ==================================================
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
// BUZZER SOUNDS (passive buzzer, uses tone()/noTone())
// ==================================================

// Short, cute two-note upward chirp for touch feedback
void playTouchBeep() {
  tone(BUZZER_PIN, 1046, 40);  // C6
  delay(45);
  tone(BUZZER_PIN, 1568, 60);  // G6
  delay(65);
  noTone(BUZZER_PIN);
}

// A gentle "heads up" descending-then-rising tune for bad AQI / rain alerts
void playAlertTune() {
  int notes[]     = { 880, 740, 880, 988 };
  int durations[]  = { 90, 90, 90, 140 };
  for (int i = 0; i < 4; i++) {
    tone(BUZZER_PIN, notes[i], durations[i]);
    delay(durations[i] + 20);
  }
  noTone(BUZZER_PIN);
}

// Descending "womp womp" for game over
void playGameOverTone() {
  tone(BUZZER_PIN, 400, 150);
  delay(160);
  tone(BUZZER_PIN, 250, 250);
  delay(260);
  noTone(BUZZER_PIN);
}

#include "Weather.h"
#include "Games.h"

// ==================================================
// DRAWING & ANIMATION — FACE
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
  // MOOD_CUTE: no mask at all — eyes stay fully round and open
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
    if (currentMood == MOOD_CUTE) {
      // extra sparkle makes the cute mood feel extra pleasant
      display.fillCircle(px + 3, py + ph - 4, 1, SSD1306_WHITE);
    }
  }

  drawEyelidMask(e.x, e.y, e.w, e.h, currentMood, isLeft);
}

void updatePhysicsAndMood() {
  unsigned long now = millis();
  breathVal = sin(now / 800.0) * 1.5;

  // --- BLINK LOGIC (skipped while holding a forced mood) ---
  if (!holdActive) {
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
      case MOOD_CUTE:
        leftEye.targetW = 44; leftEye.targetH = 44;
        rightEye.targetW = 44; rightEye.targetH = 44;
        break;
    }
  }

  leftEye.update();
  rightEye.update();
}

void drawEmoPage() {
  display.clearDisplay();

  if (holdActive) {
    currentMood = holdMood;  // forced mood while touch is held
  } else {
    updateMoodRandomly();
  }
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
// WIFI SETUP
// ==================================================
void connectWiFi() {
  display.clearDisplay();
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 28);
  display.print("Connecting WiFi...");
  display.display();

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  } else {
    Serial.print("WiFi FAILED. Status code: ");
    Serial.println(WiFi.status());
    // Status codes: 0=IDLE 1=NO_SSID_AVAIL 4=CONNECT_FAILED
    // 6=WRONG_PASSWORD 3=CONNECTED 7=DISCONNECTED
  }
}

// ==================================================
// TOUCH HANDLING
// Face/Time/Weather: tap cycles pages, hold changes Face mood.
// Game Menu: tap cycles selection, hold (~1.2s) enters the game (or exits to Face).
// In a Game: tap is the game action, hold (~1.5s) exits to the menu.
// ==================================================
const unsigned long GAME_MENU_SELECT_HOLD_MS = 1200;
const unsigned long GAME_EXIT_HOLD_MS = 1500;

void cyclePage() {
  if (currentPage == PAGE_TIME) currentPage = PAGE_WEATHER;
  else if (currentPage == PAGE_WEATHER) currentPage = PAGE_GAME_MENU;
  else if (currentPage == PAGE_FACE) {
    currentPage = PAGE_SPLASH;
    splashLoopEnterTime = millis(); // auto-advances to Time after SPLASH_LOOP_DURATION_MS
  } else {
    currentPage = PAGE_TIME; // safety fallback
  }
}

void handleTouch() {
  bool touchState = digitalRead(touchPin) == HIGH;
  unsigned long now = millis();
  if (touchState) lastActivityTime = now; // any press/hold counts as activity

  if (touchState) {
    if (!touchActive) {
      touchActive = true;
      touchStartTime = now;
      longPressTriggered = false;
    } else {
      unsigned long heldFor = now - touchStartTime;

      if (currentPage == PAGE_FACE) {
        if (heldFor > HOLD_SIDE_MS) {
          holdActive = true; holdMood = MOOD_SUSPICIOUS; longPressTriggered = true;
        } else if (heldFor > HOLD_CUTE_MS) {
          holdActive = true; holdMood = MOOD_CUTE; longPressTriggered = true;
        }
      } else if (currentPage == PAGE_GAME_MENU) {
        if (!longPressTriggered && heldFor > GAME_MENU_SELECT_HOLD_MS) {
          longPressTriggered = true;
          if (selectedMenuIndex == GAME_COUNT) {
            currentPage = PAGE_FACE;
            selectedMenuIndex = 0;
          } else {
            enterGame(selectedMenuIndex);
          }
        }
      } else if (currentPage == PAGE_GAME) {
        // Hold-to-exit only counts once the game has actually ended —
        // holding mid-game never kicks you out to the menu.
        if (currentGameOver && !longPressTriggered && heldFor > GAME_EXIT_HOLD_MS) {
          longPressTriggered = true;
          currentPage = PAGE_GAME_MENU;
        }
      }
    }
  } else {
    if (touchActive) {
      touchActive = false;
      bool wasHold = longPressTriggered;
      holdActive = false;
      lastActivityTime = now;

      if (!wasHold) {
        playTouchBeep();
        if (currentPage == PAGE_FACE || currentPage == PAGE_TIME || currentPage == PAGE_WEATHER) {
          cyclePage();
        } else if (currentPage == PAGE_SPLASH) {
          currentPage = PAGE_TIME; // tapping through the splash skips the wait
        } else if (currentPage == PAGE_GAME_MENU) {
          selectedMenuIndex = (selectedMenuIndex + 1) % (GAME_COUNT + 1);
        } else if (currentPage == PAGE_GAME) {
          if (currentGameOver) {
            restartCurrentGame();
          } else {
            actionTapThisFrame = true;
          }
        }
      }
    }
  }
}

// ==================================================
// SETUP / LOOP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // give the serial monitor time to attach
  Serial.println("\n--- Booting ---");

  pinMode(touchPin, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  randomSeed(analogRead(0));

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawSplashScreen(); // one-time blocking greeting at boot

  initEyes();
  connectWiFi();

  currentPage = PAGE_TIME; // boot flow: Splash -> Time -> Weather -> Games -> Emoji -> (loop)
  lastActivityTime = millis();
}

void loop() {
  handleTouch();
  unsigned long now = millis();

  // Idle timeout: if you've stopped touching while on Time/Weather/GameMenu,
  // settle back onto the Emoji face like a screensaver. Never applies mid-game.
  if ((currentPage == PAGE_TIME || currentPage == PAGE_WEATHER || currentPage == PAGE_GAME_MENU)
      && (now - lastActivityTime > IDLE_TO_FACE_MS)) {
    currentPage = PAGE_FACE;
  }

  // The in-loop splash (reached by tapping out of the Emoji page) is brief
  // and auto-advances to Time without needing another tap.
  if (currentPage == PAGE_SPLASH && (now - splashLoopEnterTime > SPLASH_LOOP_DURATION_MS)) {
    currentPage = PAGE_TIME;
  }

  // Refresh weather periodically, or immediately the first time
  // the weather page is opened.
  bool dueForRefresh = (now - lastWeatherFetch > WEATHER_REFRESH_MS) || !weatherDataValid;
  if (currentPage == PAGE_WEATHER && dueForRefresh) {
    fetchWeatherAndAQI();
  }

  if (currentPage == PAGE_SPLASH) {
    drawSplashPage();
  } else if (currentPage == PAGE_FACE) {
    drawEmoPage();
  } else if (currentPage == PAGE_TIME) {
    drawClock();
  } else if (currentPage == PAGE_WEATHER) {
    drawWeatherPage();
  } else if (currentPage == PAGE_GAME_MENU) {
    drawGameMenuPage();
  } else {
    drawGamePage();
  }

  actionTapThisFrame = false; // one-frame pulse consumed above; reset for next loop

  delay(20); // ~50fps cap, keeps animation smooth without hammering the bus
}