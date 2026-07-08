# 🌟 ESP32 Emoji Companion

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-IoT-blue?style=for-the-badge&logo=espressif">
  <img src="https://img.shields.io/badge/OLED-SSD1306-success?style=for-the-badge">
  <img src="https://img.shields.io/badge/PlatformIO-Framework-orange?style=for-the-badge&logo=platformio">
  <img src="https://img.shields.io/badge/C%2B%2B-Firmware-00599C?style=for-the-badge&logo=cplusplus">
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge">
</p>

<p align="center">
<b>An adorable ESP32-powered desktop companion with expressive animated eyes, WiFi connectivity, weather updates, air quality monitoring, interactive mini games, and touch-based controls—all packed into a tiny OLED display.</b>
</p>

---

# 📖 Table of Contents

* [Overview](#-overview)
* [Features](#-features)
* [Demo](#-demo)
* [Hardware](#-hardware)
* [Pin Connections](#-pin-connections)
* [Project Structure](#-project-structure)
* [Installation](#-installation)
* [Configuration](#-configuration)
* [How to Use](#-how-to-use)
* [Pages](#-pages)
* [Games](#-games)
* [Sound Effects](#-sound-effects)
* [Libraries Used](#-libraries-used)
* [Future Improvements](#-future-improvements)
* [Known Limitations](#-known-limitations)
* [Contributing](#-contributing)
* [License](#-license)

---

# 🌈 Overview

ESP32 Emoji Companion is a fun desktop gadget inspired by digital pets and smart assistants.

Using just an **ESP32**, an **SSD1306 OLED display**, **one touch sensor**, and a **passive buzzer**, it creates an expressive animated face that reacts to your touch while also acting as a smart desk assistant.

It can:

* 😊 Display animated emoji faces
* ⏰ Show the current time
* 🌦 Display live weather
* 🌫 Monitor air quality
* 🎮 Play four mini games
* 🔊 Produce sound effects and alerts

Everything is controlled using **a single touch button**, making the interface simple yet surprisingly powerful.

---

# ✨ Features

## 😊 Animated Emoji Face

The companion continuously animates expressive eyes with smooth movements.

### Idle Animations

* 👀 Natural blinking
* 🎯 Random eye movement (Saccades)
* 🌬 Gentle breathing animation
* 🎭 Automatic emotion changes every few seconds
* ✨ Floating emoji particles

### Available Emotions

| Emoji | Mood       |
| ----- | ---------- |
| 😀    | Normal     |
| 😊    | Happy      |
| 😲    | Surprised  |
| 😴    | Sleepy     |
| 😠    | Angry      |
| 😢    | Sad        |
| 🤩    | Excited    |
| ❤️    | Love       |
| 🤨    | Suspicious |
| 🥺    | Cute       |

---

## 🌐 Smart Connectivity

* WiFi support
* NTP Time Synchronization
* Automatic Weather Updates
* OpenWeatherMap API
* Air Quality Index Monitoring

---

## ⏰ Smart Clock

Displays

* Current Time
* Current Date
* AM / PM
* WiFi Status
* Time Sync Status

---

## 🌦 Weather Information

Displays

* 🌡 Temperature
* 🥵 Feels Like Temperature
* 💧 Humidity
* ☁ Weather Description
* 🌫 Air Quality Index

Weather updates refresh automatically every **10 minutes**.

---

## 🚨 Smart Alerts

The buzzer automatically alerts when:

* 🌧 Rain starts
* ⛈ Storm begins
* 🌫 AQI becomes Poor
* 😷 AQI becomes Very Poor

The alert only plays once until conditions improve.

---

## 🎮 Mini Games

Four complete games are included.

| Game            | Description                 |
| --------------- | --------------------------- |
| 🦖 Dino Runner  | Jump over obstacles         |
| 🐦 Flappy Bird  | Fly through pipes           |
| 🔨 Whack-a-Mole | Tap while mole is visible   |
| 🎵 Rhythm Tap   | Tap perfectly with the beat |

High scores remain until the board restarts.

---

# 🎥 Demo

> *(Add screenshots or GIFs here)*

Example:

```
images/
│
├── face.gif
├── weather.jpg
├── games.gif
```

---

# 🛠 Hardware

| Component      | Details            |
| -------------- | ------------------ |
| ESP32 DevKit   | ESP32-WROOM        |
| OLED Display   | SSD1306 128×64 I2C |
| Touch Sensor   | TTP223             |
| Passive Buzzer | GPIO25             |
| USB Cable      | Programming        |

---

# 🔌 Pin Connections

| Component      | ESP32 Pin |
| -------------- | --------- |
| OLED SDA       | GPIO21    |
| OLED SCL       | GPIO22    |
| Touch Sensor   | GPIO4     |
| Passive Buzzer | GPIO25    |

---

# 📂 Project Structure

```text
ESP32-Emoji-Companion
│
├── platformio.ini
│
├── src
│   └── main.cpp
│
├── Eye.h
│
├── README.md
│
└── images
    ├── face.gif
    ├── weather.jpg
    └── games.gif
```

---

# ⚙ Installation

## 1. Clone Repository

```bash
git clone https://github.com/yourusername/ESP32-Emoji-Companion.git
```

---

## 2. Open in PlatformIO

Open the project folder in Visual Studio Code with PlatformIO installed.

---

## 3. Install Libraries

PlatformIO will automatically install required libraries.

---

## 4. Configure WiFi

Inside **main.cpp**

```cpp
const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
```

---

## 5. Configure Weather API

```cpp
const char* OWM_API_KEY = "YOUR_API_KEY";
const char* WEATHER_CITY = "Ghaziabad,IN";
```

OpenWeatherMap API keys may require several hours before activation.

---

## 6. Upload

Connect the ESP32 and click **Upload**.

---

# ⚙ Configuration

If you're outside India, update

```cpp
GMT_OFFSET_SEC
```

to match your local timezone.

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
🚀 Starter
    │
    ▼
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
    └──────────► 😊 Face
```

---

# 📄 Pages

## 🚀 Starter Page

When the ESP32 powers on, DeskBuddy displays a startup screen before entering the main interface.

### Features

- 🎉 Welcome animation
- 📱 Project name ("OkayOkay")
- 📶 WiFi connection status
- ⏰ Time synchronization status
- 🌦 Weather initialization
- 😊 Smooth transition to the Face page

This provides a cleaner startup experience and lets the user know the device is initializing its services before becoming interactive.

## 😊 Face

Animated companion with multiple expressions.

---

## ⏰ Time

Shows

* Time
* Date
* AM / PM
* WiFi Status

---

## 🌦 Weather

Shows

* Temperature
* Humidity
* Feels Like
* Weather Condition
* Air Quality

---

## 🎮 Games Menu

### Tap

Move to next game.

### Hold (~1.2s)

Start selected game.

---

# 🎮 Games

## 🦖 Dino Runner

Tap to jump over obstacles.

As the score increases:

* Speed increases
* Difficulty increases

---

## 🐦 Flappy Bird

Tap to flap upward.

Avoid hitting:

* Pipes
* Ground
* Ceiling

---

## 🔨 Whack-a-Mole

Tap only while the mole is visible.

Miss five times and the game ends.

---

## 🎵 Rhythm Tap

Listen to the buzzer.

Tap exactly on the beat.

Scoring:

* PERFECT
* GOOD
* MISS

Game ends after **16 beats**.

---

# 🔊 Sound Effects

* Touch Feedback
* Menu Navigation
* Weather Alert
* AQI Alert
* Game Sounds
* Rhythm Beat

---

# 📚 Libraries Used

* WiFi
* HTTPClient
* ArduinoJson
* Adafruit SSD1306
* Adafruit GFX
* Wire
* time.h

---

# 🚀 Future Improvements

* 💾 Save scores using Preferences
* 📈 Weather Forecast API
* 🔋 Battery Monitoring
* 📱 Bluetooth App
* 🌈 RGB Mood Lighting
* 🤖 TinyML Emotion Recognition
* 🎤 Voice Commands
* 🗓 Calendar Reminders
* 📊 Activity Tracker
* 🎵 Better Audio Engine
* 💤 Sleep Mode
* 📡 OTA Firmware Updates

---

# ⚠ Known Limitations

* High scores reset after reboot.
* Weather alerts only check current weather.
* Some buzzer sounds still use `delay()`.
* Requires WiFi for weather and time synchronization.

---

# 🤝 Contributing

Contributions are welcome!

If you have ideas for:

* New games
* Better animations
* New emotions
* UI improvements
* Performance optimizations

Feel free to fork the project and submit a Pull Request.

---

# ⭐ Support

If you enjoyed this project,

🌟 **Give it a star on GitHub!**

It really helps the project grow.

---

# 📜 License

This project is licensed under the **MIT License**.

---

# 👨‍💻 Author

**Mahi Ahalawat**

Robotics & AI Engineering Student

Interested in:

* 🤖 Robotics
* 🧠 Edge AI
* 📷 Computer Vision
* 📡 IoT
* 🔬 Embedded Systems

---

<p align="center">
Made with ❤️ using ESP32, PlatformIO, and a lot of ☕
</p>
