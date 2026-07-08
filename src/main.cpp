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

// ---------------- WIFI / TIME CONFIG ----------------
const char* WIFI_SSID     = "mahi";
const char* WIFI_PASSWORD = "123456788";

const char* NTP_SERVER     = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 19800;   // IST = UTC+5:30
const int   DST_OFFSET_SEC = 0;       // India doesn't use DST

// ---------------- WEATHER / AQI CONFIG ----------------
const char* OWM_API_KEY = "0c792c9d14a7101c1ff8ce73975bd085";
const char* WEATHER_CITY = "Ghaziabad,IN";  

const unsigned long WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL; // refresh every 10 minutes
unsigned long lastWeatherFetch = 0;
bool weatherDataValid = false;

float weatherTemp = 0;
float weatherFeelsLike = 0;
int weatherHumidity = 0;
String weatherMain = "";      // e.g. "Clear", "Rain", "Clouds"
String weatherDesc = "";      // e.g. "light rain"
float weatherLat = 0, weatherLon = 0;
int currentAQI = 0;           // OpenWeatherMap scale: 1=Good 2=Fair 3=Moderate 4=Poor 5=Very Poor

bool lastAlertWasBad = false; // so we only beep the alert once per bad-condition transition

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
enum Page { PAGE_FACE, PAGE_TIME, PAGE_WEATHER, PAGE_GAME_MENU, PAGE_GAME };
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

// ==================================================
// WEATHER / AQI FETCH (OpenWeatherMap)
// ==================================================
String aqiLabel(int aqi) {
  switch (aqi) {
    case 1: return "Good";
    case 2: return "Fair";
    case 3: return "Moderate";
    case 4: return "Poor";
    case 5: return "Very Poor";
    default: return "--";
  }
}

bool isBadCondition() {
  bool badAQI = (currentAQI >= 4);
  bool rainy = (weatherMain == "Rain" || weatherMain == "Drizzle" || weatherMain == "Thunderstorm");
  return badAQI || rainy;
}

bool fetchWeatherAndAQI() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // skip certificate validation for simplicity
  HTTPClient http;

  // --- Current weather ---
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + String(WEATHER_CITY) +
               "&appid=" + String(OWM_API_KEY) + "&units=metric";
  http.begin(client, url);
  int code = http.GET();

  if (code != 200) {
    Serial.print("Weather fetch failed, HTTP code: ");
    Serial.println(code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("Weather JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  weatherTemp = doc["main"]["temp"] | 0.0;
  weatherFeelsLike = doc["main"]["feels_like"] | 0.0;
  weatherHumidity = doc["main"]["humidity"] | 0;
  weatherMain = String((const char*)(doc["weather"][0]["main"] | ""));
  weatherDesc = String((const char*)(doc["weather"][0]["description"] | ""));
  weatherLat = doc["coord"]["lat"] | 0.0;
  weatherLon = doc["coord"]["lon"] | 0.0;

  // --- AQI (needs lat/lon from the weather response) ---
  String aqiUrl = "https://api.openweathermap.org/data/2.5/air_pollution?lat=" +
                   String(weatherLat, 6) + "&lon=" + String(weatherLon, 6) +
                   "&appid=" + String(OWM_API_KEY);
  http.begin(client, aqiUrl);
  int aqiCode = http.GET();

  if (aqiCode == 200) {
    String aqiPayload = http.getString();
    DynamicJsonDocument aqiDoc(1024);
    if (!deserializeJson(aqiDoc, aqiPayload)) {
      currentAQI = aqiDoc["list"][0]["main"]["aqi"] | 0;
    }
  } else {
    Serial.print("AQI fetch failed, HTTP code: ");
    Serial.println(aqiCode);
  }
  http.end();

  weatherDataValid = true;
  lastWeatherFetch = millis();

  // Fire the alert tune only on transition into a bad condition
  bool bad = isBadCondition();
  if (bad && !lastAlertWasBad) {
    playAlertTune();
  }
  lastAlertWasBad = bad;

  return true;
}

void drawWeatherPage() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (WiFi.status() != WL_CONNECTED) {
    display.setFont(NULL);
    display.setCursor(20, 28);
    display.print("WiFi not connected");
    display.display();
    return;
  }

  if (!weatherDataValid) {
    display.setFont(NULL);
    display.setCursor(15, 28);
    display.print("Fetching weather...");
    display.display();
    return;
  }

  display.setFont(NULL);
  display.setCursor(0, 0);
  String c = String(WEATHER_CITY);
  c.toUpperCase();
  display.print(c);

  display.setFont(&FreeSansBold18pt7b);
  int tempInt = (int)weatherTemp;
  display.setCursor(0, 34);
  display.print(tempInt);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(String(tempInt).c_str(), 0, 34, &x1, &y1, &w, &h);
  display.fillCircle(x1 + w + 6, 14, 3, SSD1306_WHITE); // degree symbol
  display.setFont(NULL);
  display.setCursor(x1 + w + 12, 24);
  display.print("C");

  display.setFont(NULL);
  display.setCursor(0, 44);
  display.print("Feels ");
  display.print((int)weatherFeelsLike);

  display.setCursor(0, 54);
  display.print("Hum ");
  display.print(weatherHumidity);
  display.print("%");

  display.setCursor(70, 34);
  display.print(weatherDesc);

  display.setCursor(70, 44);
  display.print("AQI: ");
  display.print(aqiLabel(currentAQI));

  if (isBadCondition()) {
    display.setCursor(70, 54);
    display.print("! Take care");
  }

  display.display();
}

// ==================================================
// GAMES — common state
// ==================================================
enum GameID { GAME_DINO = 0, GAME_WHACK = 1, GAME_FLAPPY = 2, GAME_RHYTHM = 3, GAME_COUNT = 4 };
const char* gameNames[GAME_COUNT] = { "Dino Runner", "Whack-a-Mole", "Flappy Bird", "Rhythm Tap" };

int selectedMenuIndex = 0;      // 0..GAME_COUNT-1 = games, GAME_COUNT = "Exit to Face"
int currentGameID = GAME_DINO;

bool actionTapThisFrame = false; // one-frame pulse: the tap the current game should react to
bool currentGameOver = false;
int currentScore = 0;
int bestScore[GAME_COUNT] = { 0, 0, 0, 0 };

// -------------------- DINO RUNNER --------------------
float dinoY, dinoVelY;
bool dinoOnGround;
const float DINO_X = 14;
const float DINO_SIZE = 10;
const float GROUND_Y = 54;
const float GRAVITY = 0.6;
const float JUMP_VELOCITY = -7.5;

struct Obstacle { float x; int w, h; bool active; };
Obstacle dinoObstacles[3];
float dinoSpeed;
unsigned long lastDinoObstacleTime;
unsigned long dinoObstacleInterval;

void initDino() {
  dinoY = GROUND_Y - DINO_SIZE;
  dinoVelY = 0;
  dinoOnGround = true;
  for (int i = 0; i < 3; i++) dinoObstacles[i].active = false;
  dinoSpeed = 3.0;
  lastDinoObstacleTime = millis();
  dinoObstacleInterval = random(1200, 2200);
  currentScore = 0;
  currentGameOver = false;
}

void updateDrawDino() {
  unsigned long now = millis();
  display.clearDisplay();

  if (!currentGameOver) {
    if (actionTapThisFrame && dinoOnGround) {
      dinoVelY = JUMP_VELOCITY;
      dinoOnGround = false;
    }
    dinoVelY += GRAVITY;
    dinoY += dinoVelY;
    if (dinoY >= GROUND_Y - DINO_SIZE) {
      dinoY = GROUND_Y - DINO_SIZE;
      dinoVelY = 0;
      dinoOnGround = true;
    }

    if (now - lastDinoObstacleTime > dinoObstacleInterval) {
      lastDinoObstacleTime = now;
      dinoObstacleInterval = random(1000, 2000);
      for (int i = 0; i < 3; i++) {
        if (!dinoObstacles[i].active) {
          dinoObstacles[i].active = true;
          dinoObstacles[i].x = SCREEN_WIDTH;
          dinoObstacles[i].w = 6 + random(0, 6);
          dinoObstacles[i].h = 10 + random(0, 10);
          break;
        }
      }
    }

    for (int i = 0; i < 3; i++) {
      if (dinoObstacles[i].active) {
        dinoObstacles[i].x -= dinoSpeed;
        if (dinoObstacles[i].x + dinoObstacles[i].w < 0) {
          dinoObstacles[i].active = false;
          currentScore++;
        }
        float ox = dinoObstacles[i].x;
        int ow = dinoObstacles[i].w, oh = dinoObstacles[i].h;
        float oy = GROUND_Y - oh;
        bool overlapX = (DINO_X < ox + ow) && (DINO_X + DINO_SIZE > ox);
        bool overlapY = (dinoY < oy + oh) && (dinoY + DINO_SIZE > oy);
        if (overlapX && overlapY) {
          currentGameOver = true;
          if (currentScore > bestScore[GAME_DINO]) bestScore[GAME_DINO] = currentScore;
          playGameOverTone();
        }
      }
    }
    dinoSpeed += 0.0015;
  }

  display.drawLine(0, GROUND_Y, SCREEN_WIDTH, GROUND_Y, SSD1306_WHITE);
  display.fillRect(DINO_X, dinoY, DINO_SIZE, DINO_SIZE, SSD1306_WHITE);
  for (int i = 0; i < 3; i++) {
    if (dinoObstacles[i].active) {
      float oy = GROUND_Y - dinoObstacles[i].h;
      display.fillRect((int)dinoObstacles[i].x, (int)oy, dinoObstacles[i].w, dinoObstacles[i].h, SSD1306_WHITE);
    }
  }

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print(currentScore);

  if (currentGameOver) {
    display.setCursor(28, 20); display.print("GAME OVER");
    display.setCursor(20, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 44); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- FLAPPY BIRD --------------------
float birdY, birdVelY;
const float BIRD_X = 20;
const int BIRD_SIZE = 6;
const float F_GRAVITY = 0.35;
const float FLAP_VELOCITY = -4.2;
const int PIPE_WIDTH = 10;
const int GAP_HEIGHT = 24;

struct Pipe { float x; int gapY; bool active; bool scored; };
Pipe pipes[3];
float pipeSpeed;
unsigned long lastPipeSpawn, pipeSpawnInterval;

void initFlappy() {
  birdY = SCREEN_HEIGHT / 2;
  birdVelY = 0;
  for (int i = 0; i < 3; i++) pipes[i].active = false;
  pipeSpeed = 2.2;
  lastPipeSpawn = millis();
  pipeSpawnInterval = 1600;
  currentScore = 0;
  currentGameOver = false;
}

void updateDrawFlappy() {
  unsigned long now = millis();
  display.clearDisplay();

  if (!currentGameOver) {
    if (actionTapThisFrame) birdVelY = FLAP_VELOCITY;
    birdVelY += F_GRAVITY;
    birdY += birdVelY;

    if (birdY < 0) { birdY = 0; birdVelY = 0; }
    if (birdY > SCREEN_HEIGHT - BIRD_SIZE) {
      currentGameOver = true;
      if (currentScore > bestScore[GAME_FLAPPY]) bestScore[GAME_FLAPPY] = currentScore;
      playGameOverTone();
    }

    if (now - lastPipeSpawn > pipeSpawnInterval) {
      lastPipeSpawn = now;
      for (int i = 0; i < 3; i++) {
        if (!pipes[i].active) {
          pipes[i].active = true;
          pipes[i].scored = false;
          pipes[i].x = SCREEN_WIDTH;
          pipes[i].gapY = random(10, SCREEN_HEIGHT - GAP_HEIGHT - 10);
          break;
        }
      }
    }

    for (int i = 0; i < 3; i++) {
      if (pipes[i].active) {
        pipes[i].x -= pipeSpeed;
        if (pipes[i].x + PIPE_WIDTH < 0) pipes[i].active = false;

        bool overlapX = (BIRD_X < pipes[i].x + PIPE_WIDTH) && (BIRD_X + BIRD_SIZE > pipes[i].x);
        bool inGap = (birdY > pipes[i].gapY) && (birdY + BIRD_SIZE < pipes[i].gapY + GAP_HEIGHT);
        if (overlapX && !inGap && !currentGameOver) {
          currentGameOver = true;
          if (currentScore > bestScore[GAME_FLAPPY]) bestScore[GAME_FLAPPY] = currentScore;
          playGameOverTone();
        }
        if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH < BIRD_X) {
          pipes[i].scored = true;
          currentScore++;
        }
      }
    }
  }

  for (int i = 0; i < 3; i++) {
    if (pipes[i].active) {
      display.fillRect((int)pipes[i].x, 0, PIPE_WIDTH, pipes[i].gapY, SSD1306_WHITE);
      display.fillRect((int)pipes[i].x, pipes[i].gapY + GAP_HEIGHT, PIPE_WIDTH,
                        SCREEN_HEIGHT - (pipes[i].gapY + GAP_HEIGHT), SSD1306_WHITE);
    }
  }
  display.fillRoundRect(BIRD_X, (int)birdY, BIRD_SIZE, BIRD_SIZE, 2, SSD1306_WHITE);

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print(currentScore);

  if (currentGameOver) {
    display.setCursor(28, 20); display.print("GAME OVER");
    display.setCursor(20, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 44); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- WHACK-A-MOLE --------------------
// Only one input exists, so "aiming" isn't possible — the challenge is timing:
// a mole pops up at a random spot for a short window and you tap while it's up.
int wMolePos = -1;
unsigned long wMoleShowTime, wMoleVisibleDuration, wNextMoleTime;
int wMisses = 0;
const int W_MAX_MISSES = 5;
bool wMoleActiveVisible = false;

void initWhack() {
  currentScore = 0;
  currentGameOver = false;
  wMisses = 0;
  wMoleActiveVisible = false;
  wNextMoleTime = millis() + random(500, 1500);
}

void updateDrawWhack() {
  unsigned long now = millis();
  display.clearDisplay();

  if (!currentGameOver) {
    if (!wMoleActiveVisible && now >= wNextMoleTime) {
      wMoleActiveVisible = true;
      wMolePos = random(0, 3);
      wMoleShowTime = now;
      wMoleVisibleDuration = random(600, 1000);
    }

    if (wMoleActiveVisible && now - wMoleShowTime > wMoleVisibleDuration) {
      wMoleActiveVisible = false;
      wMisses++;
      wNextMoleTime = now + random(500, 1500);
    }

    if (actionTapThisFrame) {
      if (wMoleActiveVisible) {
        currentScore++;
        wMoleActiveVisible = false;
        wNextMoleTime = now + random(500, 1500);
      } else {
        wMisses++; // false swing
      }
    }

    if (wMisses >= W_MAX_MISSES) {
      currentGameOver = true;
      if (currentScore > bestScore[GAME_WHACK]) bestScore[GAME_WHACK] = currentScore;
      playGameOverTone();
    }
  }

  int holeX[3] = { 20, 58, 96 };
  for (int i = 0; i < 3; i++) display.drawCircle(holeX[i], 40, 12, SSD1306_WHITE);
  if (wMoleActiveVisible) display.fillCircle(holeX[wMolePos], 40, 9, SSD1306_WHITE);

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print("Score "); display.print(currentScore);
  display.setCursor(78, 2);
  display.print("Miss "); display.print(wMisses); display.print("/"); display.print(W_MAX_MISSES);

  if (currentGameOver) {
    display.setCursor(28, 20); display.print("GAME OVER");
    display.setCursor(20, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 55); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- RHYTHM TAP --------------------
const unsigned long BEAT_INTERVAL = 600;
const int RHYTHM_TOTAL_BEATS = 16;
unsigned long rhythmStartTime;
int beatsElapsed = 0;
String lastJudgement = "";
unsigned long lastJudgementTime = 0;

void initRhythm() {
  currentScore = 0;
  currentGameOver = false;
  rhythmStartTime = millis();
  beatsElapsed = 0;
  lastJudgement = "";
  lastJudgementTime = 0;
}

void updateDrawRhythm() {
  unsigned long now = millis();
  display.clearDisplay();
  unsigned long elapsed = now - rhythmStartTime;

  if (!currentGameOver) {
    int expectedBeatIndex = elapsed / BEAT_INTERVAL;

    if (expectedBeatIndex > beatsElapsed) {
      beatsElapsed = expectedBeatIndex;
      tone(BUZZER_PIN, 1000, 80); // short metronome click, non-blocking
      if (beatsElapsed >= RHYTHM_TOTAL_BEATS) {
        currentGameOver = true;
        if (currentScore > bestScore[GAME_RHYTHM]) bestScore[GAME_RHYTHM] = currentScore;
      }
    }

    if (actionTapThisFrame && !currentGameOver) {
      long phase = elapsed % BEAT_INTERVAL;
      long diff = min(phase, (long)BEAT_INTERVAL - phase);
      if (diff < 60) { currentScore += 3; lastJudgement = "PERFECT"; }
      else if (diff < 150) { currentScore += 1; lastJudgement = "GOOD"; }
      else { lastJudgement = "MISS"; }
      lastJudgementTime = now;
    }
  }

  long phase = elapsed % BEAT_INTERVAL;
  int pulse = 6 + (int)(6.0 * (1.0 - (float)phase / BEAT_INTERVAL));
  display.drawCircle(64, 30, pulse, SSD1306_WHITE);

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print("Beat "); display.print(beatsElapsed); display.print("/"); display.print(RHYTHM_TOTAL_BEATS);
  display.setCursor(2, 54);
  display.print("Score "); display.print(currentScore);

  if (now - lastJudgementTime < 400 && lastJudgement != "") {
    display.setCursor(40, 44);
    display.print(lastJudgement);
  }

  if (currentGameOver) {
    display.setCursor(34, 20); display.print("DONE!");
    display.setCursor(15, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 44); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- GAME DISPATCH / MENU --------------------
void restartCurrentGame() {
  switch (currentGameID) {
    case GAME_DINO:   initDino();   break;
    case GAME_WHACK:  initWhack();  break;
    case GAME_FLAPPY: initFlappy(); break;
    case GAME_RHYTHM: initRhythm(); break;
  }
}

void drawGamePage() {
  switch (currentGameID) {
    case GAME_DINO:   updateDrawDino();   break;
    case GAME_WHACK:  updateDrawWhack();  break;
    case GAME_FLAPPY: updateDrawFlappy(); break;
    case GAME_RHYTHM: updateDrawRhythm(); break;
  }
}

void enterGame(int id) {
  currentGameID = id;
  currentPage = PAGE_GAME;
  restartCurrentGame();
}

void drawGameMenuPage() {
  display.clearDisplay();
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(38, 0);
  display.print("GAMES");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  for (int i = 0; i < GAME_COUNT; i++) {
    int y = 12 + i * 9;
    if (i == selectedMenuIndex) {
      display.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y);
    display.print(gameNames[i]);
    display.setCursor(90, y);
    display.print(bestScore[i]);
  }
  display.setTextColor(SSD1306_WHITE);

  int y = 12 + GAME_COUNT * 9;
  if (selectedMenuIndex == GAME_COUNT) {
    display.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }
  display.setCursor(4, y);
  display.print("Exit to Face");
  display.setTextColor(SSD1306_WHITE);

  display.display();
}

// ==================================================
// BOOT SPLASH SCREEN
// ==================================================
void drawSplashScreen() {
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

  display.display();
  delay(2200);
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
  if (currentPage == PAGE_FACE) currentPage = PAGE_TIME;
  else if (currentPage == PAGE_TIME) currentPage = PAGE_WEATHER;
  else if (currentPage == PAGE_WEATHER) currentPage = PAGE_GAME_MENU;
  else currentPage = PAGE_FACE;
}

void handleTouch() {
  bool touchState = digitalRead(touchPin) == HIGH;
  unsigned long now = millis();

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
        if (!longPressTriggered && heldFor > GAME_EXIT_HOLD_MS) {
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

      if (!wasHold) {
        playTouchBeep();
        if (currentPage == PAGE_FACE || currentPage == PAGE_TIME || currentPage == PAGE_WEATHER) {
          cyclePage();
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

  drawSplashScreen();

  initEyes();
  connectWiFi();

  currentPage = PAGE_FACE;
}

void loop() {
  handleTouch();

  // Refresh weather periodically, or immediately the first time
  // the weather page is opened.
  unsigned long now = millis();
  bool dueForRefresh = (now - lastWeatherFetch > WEATHER_REFRESH_MS) || !weatherDataValid;
  if (currentPage == PAGE_WEATHER && dueForRefresh) {
    fetchWeatherAndAQI();
  }

  if (currentPage == PAGE_FACE) {
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