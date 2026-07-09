#ifndef WEATHER_H
#define WEATHER_H

// Weather + AQI page (OpenWeatherMap). Relies on `display`, SCREEN_WIDTH,
// WiFi/HTTPClient/WiFiClientSecure/ArduinoJson, BUZZER_PIN, and
// playAlertTune() already being available — include this from main.cpp
// AFTER those are set up.

// ---------------- WEATHER / AQI CONFIG ----------------
// ---------------- WEATHER / AQI CONFIG ----------------
const char* OWM_API_KEY = "your_openweathermap_api_key";  // replace with your OpenWeatherMap API key
const char* WEATHER_CITY = "Ghaziabad,IN";  // change to "Delhi,IN" if you'd rather use Delhi proper

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

  // Simple stacked layout — one row each, no shared x/y coordinates,
  // so nothing can overlap regardless of text length.
  display.setFont(NULL);

  String c = String(WEATHER_CITY);
  c.toUpperCase();
  display.setCursor(0, 0);
  display.print(c);

  display.setCursor(0, 14);
  display.print(weatherDesc);

  display.setCursor(0, 28);
  display.print("AQI: ");
  display.print(aqiLabel(currentAQI));

  display.setFont(&FreeSansBold18pt7b);
  int tempInt = (int)weatherTemp;
  char tempStr[8];
  sprintf(tempStr, "%dC", tempInt);
  display.setCursor(0, 60);
  display.print(tempStr);

  display.display();
}


#endif