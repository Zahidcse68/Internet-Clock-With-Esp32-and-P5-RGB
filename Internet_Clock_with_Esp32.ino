//Created By Tech Zaid 
//youtube https://www.youtube.com/@techzaiduu
//instgram @kashurengineer

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include "time.h"
#include <AsyncHTTPRequest_Generic.h>
#include <ArduinoJson.h>
#include <Fonts\FreeSansBold9pt7b.h>
#include "clockFont.h"

// Change these to whatever suits
const char* ssid = "iphone";
const char* password = "12345678";

const long gmtOffset_sec = 19800; // Time zone offset in seconds
const int daylightOffset_sec = 0;
String latitude = "00000";  //enter latitude
String longitude = "00000";  //longitude
String apiKey = "your api key";

#define R1_PIN 19
#define G1_PIN 13
#define B1_PIN 18
#define R2_PIN 5
#define G2_PIN 12
#define B2_PIN 17

#define A_PIN 16
#define B_PIN 14
#define C_PIN 4
#define D_PIN 27
#define E_PIN -1

#define LAT_PIN 26
// --- CHANGED PINS TO AVOID ESP32 STRAPPING PIN INTERFERENCE ---
#define OE_PIN 22  // Previously 15
#define CLK_PIN 23 // Previously 2
// --------------------------------------------------------------

const char* ntpServer = "pool.ntp.org";
String serverName = "http://api.openweathermap.org/data/2.5/weather?lat=" + latitude + "&lon=" + longitude + "&appid=" + apiKey + "&units=metric";

HUB75_I2S_CFG::i2s_pins _pins = { R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN };

// Declare the display pointer globally so all functions can use it
MatrixPanel_I2S_DMA* dma_display = nullptr;

AsyncHTTPRequest request;
bool readWeather = false;
bool weatherUpdated = false;
const char* weather_0_main;
double main_tempF;
String weather, temperature;
int prevMin = 0, prevHour = 0, prevSecond = 0;
char* daysList[] = { "DAY", "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "WEDNESDAY", "SATURDAY" };

void setup() {
  Serial.begin(115200);

  // --- ANTI-GHOSTING CONFIGURATION ---
  HUB75_I2S_CFG mxconfig(
    64,    // Module width
    32,    // Module height
    1,     // chain length
    _pins  // pin mapping
  );
  
  // These two lines are the software fix for left-side ghosting
  mxconfig.latch_blanking = 6; // Gives the panel time to switch rows before sending data
  mxconfig.clkphase = false;   // Reverses clock phase to align data perfectly
  
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();             // setup the LED matrix
  // -----------------------------------

  dma_display->setBrightness8(255);  // 0-255
  dma_display->setTextWrap(false);  // Don't wrap at end of line - will do ourselves
  dma_display->clearScreen();
  dma_display->setTextColor(dma_display->color444(15, 15, 0));

  // Connect to Wi-Fi
  dma_display->clearScreen();
  dma_display->println("Connecting to ");
  dma_display->print(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    dma_display->print(".");
  }

  dma_display->clearScreen();
  dma_display->setCursor(0, 0);
  dma_display->println("WiFi connected.");
  dma_display->clearScreen();

  weather = "WEATHER";

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  request.onReadyStateChange(requestCB);
  sendWeatherRequest();

  uint16_t lineColor = dma_display->color565(100, 100, 100);
  dma_display->drawFastHLine(0, 7, 64, lineColor);
  dma_display->drawFastHLine(0, 23, 64, lineColor);
}

void loop() {
  delay(1000);
  printLocalTime();
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  if (prevMin > 58) prevMin = 0;
  if (prevHour > 22) prevHour = 0;

  uint16_t blank = dma_display->color444(0, 0, 0);  // 0 -15
  uint16_t colorHour = dma_display->color565(255, 255, 0);
  uint16_t colorSeconds = dma_display->color565(255, 171, 0);
  uint16_t colorWeather = dma_display->color565(192, 255, 194);
  uint16_t colortemp = dma_display->color565(0, 230, 230);
  uint16_t colorDay = dma_display->color565(100, 200, 0);

  if (weatherUpdated) {
    dma_display->fillRect(0, 0, 64, 6, blank);
    dma_display->setFont(&TTFont);
    dma_display->setTextColor(colorWeather);
    dma_display->setCursor(0, 5);
    dma_display->print(weather);
    dma_display->setCursor(47, 5);
    dma_display->setTextColor(colortemp);
    dma_display->print(temperature);
    dma_display->print("`C");

    dma_display->setFont(&TechTalkies);
    dma_display->setCursor(35, 7);
    dma_display->setTextColor(dma_display->color565(100, 0, 255));
    dma_display->print("|");

    weatherUpdated = false;
  }

  if (prevHour < timeinfo.tm_hour) {
    prevHour = timeinfo.tm_hour;

    dma_display->fillRect(3, 9, 24, 13, blank);
    dma_display->setFont(&FreeSansBold9pt7b);
    dma_display->setTextColor(colorHour);
    dma_display->setCursor(3, 21);
    dma_display->print(&timeinfo, "%I:");

    dma_display->fillRect(48, 16, 29, 6, blank);
    dma_display->setFont(&TTFont);
    dma_display->setCursor(48, 21);
    dma_display->setTextColor(colorSeconds);
    if (timeinfo.tm_hour < 13)
      dma_display->print("Am");
    else
      dma_display->print("Pm");

    dma_display->setTextColor(colorDay);
    dma_display->fillRect(0, 26, 64, 7, blank);
    dma_display->setCursor(0, 30);
    dma_display->print(daysList[timeinfo.tm_wday]);
    dma_display->setCursor(54, 30);
    char dateToday[3] = "";
    sprintf(dateToday, "%0d", timeinfo.tm_mday);
    dma_display->print(dateToday);

    // Weather updated every 1 hour
    sendWeatherRequest();
  }
  if (prevMin < timeinfo.tm_min) {
    prevMin = timeinfo.tm_min;
    dma_display->fillRect(28, 9, 20, 13, blank);
    dma_display->setFont(&FreeSansBold9pt7b);
    dma_display->setTextColor(colorHour);
    dma_display->setCursor(27, 21);
    dma_display->print(&timeinfo, "%M");
  }

  dma_display->setFont();
  dma_display->setCursor(48, 9);
  dma_display->fillRect(48, 9, 23, 7, blank);
  dma_display->setTextColor(colorSeconds);
  dma_display->print(&timeinfo, "%S");
}

void sendWeatherRequest() {
  static bool requestOpenResult;

  if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone) {
    requestOpenResult = request.open("GET", serverName.c_str());

    if (requestOpenResult) {
      // Only send() if open() returns true, or crash
      request.send();
    } else {
      Serial.println(F("Can't send bad request"));
    }
  } else {
    Serial.println(F("Not ready for request"));
  }
  readWeather = false;
}

void requestCB(void* optParm, AsyncHTTPRequest* request, int readyState) {
  (void)optParm;

  if (readyState == readyStateDone) {
    if (request->responseHTTPcode() == 200) {
      String payload = request->responseText();
      Serial.println(F("\n**************************************"));
      Serial.println(payload);
      Serial.println(F("**************************************"));

      // Allocate JsonBuffer
      // Use arduinojson.org/assistant to compute the capacity.
      const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 1024;
      DynamicJsonDocument doc(capacity);

      // Parse JSON object
      JsonObject root = doc.as<JsonObject>();

      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.println(F("Parsing failed!"));
        Serial.println(error.c_str());
        return;
      }

      // Decode JSON/Extract values
      const char* value = doc["visibility"];
      readWeather = false;

      JsonObject weather_0 = doc["weather"][0];
      weather_0_main = weather_0["main"];  // "Haze"
      JsonObject main = doc["main"];
      main_tempF = main["temp"];  // 301.86

      Serial.print("weather,");
      Serial.print(weather_0_main);
      Serial.print(",");
      Serial.println(main_tempF);

      weather = weather_0_main;
      weather.toUpperCase();
      temperature = String(int(main_tempF));
      weatherUpdated = true;
    }
  }
}
