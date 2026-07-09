# 🌟 DeskBuddy – ESP32 Emoji Companion

An interactive desktop companion powered by an ESP32 that brings personality to your workspace. Featuring expressive animated eyes, a smart splash screen, live time, weather and air quality updates, audio feedback, and four touch-controlled mini-games—all displayed on a 128×64 OLED and controlled using a single touch sensor.

---

# 🛠 Hardware

| Component           | Notes                                           |
| ------------------- | ----------------------------------------------- |
| ESP32 Dev Board     | Generic ESP32-WROOM (`esp32dev`)                |
| SSD1306 OLED        | 128×64 I²C Display (SDA = GPIO21, SCL = GPIO22) |
| TTP223 Touch Sensor | Digital Output, GPIO4                           |
| Passive Buzzer      | GPIO25                                          |

> **Note:** The ESP32 only supports **2.4 GHz WiFi** networks. 5 GHz networks are not supported.

---

# 📁 Project Structure

```text
├── platformio.ini        # PlatformIO configuration
└── src/
    ├── main.cpp          # Core application, page flow, touch handling
    ├── Eye.h             # Eye animation engine and moods
    ├── Splash.h          # Boot greeting and splash screen
    ├── Clock.h           # NTP clock
    ├── Weather.h         # Weather & AQI fetching
    └── Games.h           # Game menu and all mini-games
```

All source files are located inside the `src/` directory.

---

# ⚙ Setup

### WiFi Configuration

Edit **main.cpp**

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

### Weather Configuration

Edit **Weather.h**

```cpp
const char* OWM_API_KEY  = "YOUR_OPENWEATHERMAP_API_KEY";
const char* WEATHER_CITY = "Ghaziabad,IN";
```

A newly created OpenWeatherMap API key may take several hours (sometimes up to 24 hours) before it becomes active.

If you're outside India, update `GMT_OFFSET_SEC` inside `main.cpp` to match your local timezone.

---

# 🖥 How to Use

## Face Page

| Action         | Result          |
| -------------- | --------------- |
| Tap            | Next Page       |
| Hold 3 seconds | Cute Mode       |
| Hold 5 seconds | Suspicious Mode |
| Release        | Return to Idle  |

---

## Navigation

```
😊 Face
     │
     ▼
⏰ Time
     │
     ▼
🌦 Weather
     │
     ▼
🎮 Games
     │
     └──────────► Face
```

---

# 📄 Pages

## 😊 Face

Animated companion with multiple expressions.

---

## ⏰ Time

Shows

* Time
* Weather
* Games Menu

DeskBuddy automatically returns to the Emoji page, acting as a screensaver.

This timeout is disabled while playing a game.

---

# 😊 Emoji Face

The Emoji page is the heart of DeskBuddy, bringing the companion to life through smooth animations and expressive emotions.

## Idle Animations

* 👀 Natural blinking
* 🎯 Random eye movements (Saccades)
* 🌬 Gentle breathing animation
* 🎭 Automatic mood changes every 6–15 seconds
* ✨ Floating emotion particles

### Available Emotions

* 😀 Normal
* 😊 Happy
* 😲 Surprised
* 😴 Sleepy
* 😠 Angry
* 😢 Sad
* 🤩 Excited
* ❤️ Love
* 🤨 Suspicious

Each emotion uses its own eye shape, eyelid style, and optional particle effects.

## Touch Gestures

| Gesture         | Action                      |
| --------------- | --------------------------- |
| Tap             | Continue to Splash → Time   |
| Hold >3 seconds | Cute Mode                   |
| Hold >5 seconds | Suspicious / Side-eye Mode  |
| Release         | Return to normal animations |

---

# ⏰ Smart Clock

The Time page synchronizes with an NTP server over WiFi.

Displays:

* Current Time (12-hour format)
* AM / PM
* Current Date

If WiFi is unavailable, helpful status messages such as **"WiFi Not Connected"** or **"Syncing Time..."** are displayed.

---

# 🌦 Weather & Air Quality

Weather information is retrieved from **OpenWeatherMap** every 10 minutes (or immediately the first time the page is opened).

The layout is intentionally minimal and easy to read.

Displayed information includes:

* 📍 City
* ☁ Weather Condition
* 🌡 Temperature
* 🌫 Air Quality (Good → Very Poor)

---

# 🔊 Audio Feedback

DeskBuddy provides sound feedback through the passive buzzer.

### Touch Feedback

Every successful tap plays a pleasant two-note confirmation sound.

### Weather Alerts

A warning melody automatically plays when:

* Rain begins
* Drizzle begins
* Thunderstorms begin
* AQI becomes Poor
* AQI becomes Very Poor

The alert only triggers once whenever conditions change, preventing repeated notifications.

---

# 🎮 Mini Games

All games are designed around a single touch button.

## Controls

### Games Menu

| Gesture       | Action                 |
| ------------- | ---------------------- |
| Tap           | Move through game list |
| Hold (~1.2 s) | Start highlighted game |
| Exit to Face  | Returns to Emoji page  |

Best scores are remembered until the ESP32 is restarted.

### During Gameplay

* Tap performs the game's primary action.
* Holding while actively playing has no special function and simply counts as a normal tap when released.
* After Game Over:

  * Tap → Restart
  * Hold (~1.5 s) → Return to Games Menu

---

## Included Games

| Game            | Controls          | Description                                                                 |
| --------------- | ----------------- | --------------------------------------------------------------------------- |
| 🦖 Dino Runner  | Tap to Jump       | Avoid incoming obstacles that gradually increase in speed.                  |
| 🐦 Flappy Bird  | Tap to Flap       | Fly through randomly generated pipe gaps without crashing.                  |
| 🔨 Whack-a-Mole | Tap While Visible | Hit the mole before it disappears. Five misses end the game.                |
| 🎵 Rhythm Tap   | Tap on Beat       | Match the buzzer rhythm for PERFECT, GOOD, or MISS judgments over 16 beats. |

---

# ⚠ Known Limitations

* Weather alerts only use current conditions and do not predict future weather.
* High scores are stored only in RAM and reset after reboot.
* Some buzzer sounds still use blocking `delay()` calls, which may introduce slight timing delays.
* The 10-second idle timeout can be adjusted by changing the `IDLE_TO_FACE_MS` constant inside `main.cpp`.

---

# 🚀 Future Improvements

* Persistent high scores using Preferences/EEPROM
* Weather forecast support
* Battery monitoring
* Bluetooth companion app
* TinyML-powered emotion recognition
* Voice notifications
* Additional mini-games
* OTA firmware updates
* RGB mood lighting
* Animated boot logo
