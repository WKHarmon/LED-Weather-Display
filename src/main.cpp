#include <FastLED.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <Wire.h>
using namespace std;

const char ssid[] = "EDITME";        // your network SSID (name)
const char pass[] = "EDITME";    // your network password (use for WPA, or use as key for WEP)

#define READ_TIMEOUT 15 // Cancel query if no data received (seconds)
#define WIFI_TIMEOUT 60 // in seconds
#define RETRY_TIMEOUT 15000 // in ms
#define REQUEST_INTERVAL 3600000 // in ms (1 hour is 3600000)
#define BRIGHTNESS_INTERVAL 5000 // interval at which to adjust brightness in ms

#define FASTLED_ESP8266_RAW_PIN_ORDER
#define DATA_PIN 14
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define MAX_BRIGHTNESS 20
#define MIN_BRIGHTNESS 5
#define MIN_LUX 2 // not lux, just a luminosity reading
#define MAX_LUX 50
#define NUM_LEDS 128
CRGB leds[NUM_LEDS];

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

//range is 40 degrees blue, 70 degrees white, 90 degrees red; colors are indexed
/*DEFINE_GRADIENT_PALETTE( temperature_gradient ) {
  0,     0,  0,  255,   //blue
153,   255,  255,  255,   //white
255,   255, 0, 0 }; //red*/

// icons from https://github.com/caternuson/rpi-weather/blob/master/led8x8icons.py
// 8x8 icon editor https://xantorohara.github.io/led-matrix-editor/
byte weather_unknown[] = { // 0x8142241818244281
  0b10000001,
  0b01000010,
  0b00100100,
  0b00011000,
  0b00011000,
  0b00100100,
  0b01000010,
  0b10000001};
byte weather_clear[] = { // 0x9142183dbc184289
  0b10010001,
  0b01000010,
  0b00011000,
  0b00111101,
  0b10111100,
  0b00011000,
  0b01000010,
  0b10001001};
byte weather_few[] = { // 0x7e8191718ea0c0f0
  0b00001111,
  0b00000011,
  0b00000101,
  0b01110001,
  0b10001110,
  0b10001001,
  0b10000001,
  0b01111110};
byte weather_sct[] = { // 0x7e8191718ea0c0f0
  0b00001111,
  0b00000011,
  0b00000101,
  0b01110001,
  0b10001110,
  0b10001001,
  0b10000001,
  0b01111110};
byte weather_bkn[] = { // 0x00007e818999710e
  0b01110000,
  0b10001110,
  0b10011001,
  0b10010001,
  0b10000001,
  0b01111110,
  0b00000000,
  0b00000000};
byte weather_shower[] = { // 0x152a7e818191710e
  0b01110000,
  0b10001110,
  0b10001001,
  0b10000001,
  0b10000001,
  0b01111110,
  0b01010100,
  0b10101000};
byte weather_rain[] = { // 0x55aa55aa55aa55aa
  0b01010101,
  0b10101010,
  0b01010101,
  0b10101010,
  0b01010101,
  0b10101010,
  0b01010101,
  0b10101010};
byte weather_thunderstorm[] = { // 0x0a04087e8191710e
  0b01110000,
  0b10001110,
  0b10001001,
  0b10000001,
  0b01111110,
  0b00010000,
  0b00100000,
  0b01010000};
byte weather_snow[] = { // 0xa542a51818a542a5
  0b10100101,
  0b01000010,
  0b10100101,
  0b00011000,
  0b00011000,
  0b10100101,
  0b01000010,
  0b10100101};
byte weather_mist[] = { // 0x00ff00ff00ff00ff
  0b11111111,
  0b00000000,
  0b11111111,
  0b00000000,
  0b11111111,
  0b00000000,
  0b11111111,
  0b00000000};

short int temp;
String conditions;

#define HOST "api.weather.gov"
#define ALAMEDA_URI "/gridpoints/MTR/94,125/forecast"
#define SF_URI "/gridpoints/MTR/88,126/forecast"

unsigned int loop_number = 0;

void setup() {
  // put your setup code here, to run once:
  delay(3000);
  Serial.begin(74880);
  while (!Serial) {
    // wait
  }

  if(!tsl.begin()) {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.println("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
  } else {
    tsl.setGain(TSL2561_GAIN_16X);
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
  }

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  fill_gradient_RGB(leds, NUM_LEDS, CRGB::Blue, CRGB::Red);
  FastLED.show();
}

void draw(byte city, CRGB color, byte * image){
  unsigned short int led;
  if (city == 0) led = 0;
  else if (city == 1) led = 64;

  for (byte i = 0; i < 8; i++) {
    for (byte a = 0; a < 8; a++) {
      if (image[i] >> a & 0x01) leds[led] = color;
      led++;
    }
  }
}

CRGB getColor(float highTemp) {
  CRGB color;

  // FastLED gradients crash the ESP8266 so we have to do it manually. There's probably a math function I'm too lazy to look up.
  // http://www.perbang.dk/rgbgradient/

  if (highTemp <= 55) color = 0x0000FF;
  else if (highTemp <= 60) color = 0x3F3FFF;
  else if (highTemp <= 65) color = 0x7F7FFF;
  else if (highTemp <= 70) color = 0xBFBFFF;
  else if (highTemp <= 75) color = 0xFFFFFF;
  else if (highTemp <= 80) color = 0xFFCCCC;
  else if (highTemp <= 85) color = 0xFF9999;
  else if (highTemp <= 90) color = 0xFF6666;
  else if (highTemp <= 95) color = 0xFF3333;
  else color = 0xFF0000;
  return color;
}

void doLEDs(unsigned short int city) {
  byte * image;
  image = weather_unknown;

  // overwrite image based on priority
  if (conditions.indexOf("Sunny") >= 0) image = weather_clear;
  if (conditions.indexOf("Cloudy") >= 0) image = weather_bkn;
  if (conditions.indexOf("Mostly Sunny") >= 0) image = weather_few;
  if (conditions.indexOf("Party Cloudy") >= 0) image = weather_sct;
  if (conditions.indexOf("Partly Sunny") >= 0) image = weather_bkn;
  if (conditions.indexOf("Mostly Cloudy") >= 0) image = weather_bkn;
  if (conditions.indexOf("Smoke") >= 0) image = weather_mist;
  if (conditions.indexOf("Fog") >= 0) image = weather_mist;
  if (conditions.indexOf("Haze") >= 0) image = weather_mist;
  if (conditions.indexOf("Rain") >= 0) image = weather_rain;
  if (conditions.indexOf("Showers") >= 0) image = weather_shower;
  if (conditions.indexOf("Snow") >= 0) image = weather_snow;
  if (conditions.indexOf("Thunderstorm") >= 0) image = weather_thunderstorm;

  CRGB color;

  color = getColor(temp);
  draw(city, color, image);
  FastLED.show();
}

String getWeatherData(String url) {
  String payload = "{\"periods\": [";
  uint32_t t;
  char c;

  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  //Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (!client.connect(HOST, 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return "E";
  } else {
  /*  Serial.println("Connected ...");
    Serial.print("GET ");
    Serial.print(url);
    Serial.println(" HTTP/1.1");
    Serial.print("Host: ");
    Serial.println(HOST);
    Serial.println("User-Agent: Weather Clock client - contact kyle@kyleharmon.com");
    Serial.println("Accept: application/ld+json");
    Serial.println("Connection: close");
    Serial.println();*/
    // Make a HTTP request, and print it to console:
    client.print("GET ");
    client.print(url);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(HOST);
    client.println("User-Agent: Weather Clock client - contact kyle@kyleharmon.com");
    client.println("Accept: application/ld+json");
    client.println("Connection: close");
    client.println();
    client.flush();
    t = millis(); // start time
    
    //Serial.print("Getting data");

    while (!client.connected()) {
      if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        client.stop();
        return "E";
      }
      Serial.print(".");
      delay(1000);
    }

    unsigned short int elementCount = 0;
    String currentLine = "";
    bool readingData = false;

    while (client.connected()) {
      if ((c = client.read()) >= 0) {
        yield();
        currentLine += c;
        if (c == '\n') {
          currentLine = "";
        }
        if (currentLine.endsWith("\"periods\": [")) {
          readingData = true;
        } else if (readingData) {
          payload += c;
          if (currentLine.endsWith("}")) elementCount++;
        }
        if (elementCount >= 3) {
          client.stop();
          payload += "]}";
          return payload;
        }; // stop after we have three elements

        t = millis(); // Reset timeout clock
      } else if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        client.stop();
        return "E";
      }
    }
  }
  return "E";
}

void parsePaket(JsonObject& liste) {
  temp = liste["temperature"]; // 20.31
  Serial.print("Temp: ");
  Serial.print(temp);

  conditions = liste["shortForecast"].as<char*>();
  Serial.print(" Conditions: ");
  Serial.println(conditions);
}

void decodeWeather(String JSONline) {
  const size_t bufferSize = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(1) + 3*JSON_OBJECT_SIZE(13) + 1370;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  const char* json = JSONline.c_str();
  JsonObject& root = jsonBuffer.parseObject(json);
  JsonArray& list = root["periods"];
  for (int i = 0; i < list.size(); i++) {
    if (list[i]["isDaytime"]) {
      parsePaket(list[i]);
      break; // exit loop once we find a daytime
    }
  }
}

void setBrightness() {
 // set LED brightness
  uint16_t reading;
  uint16_t ir;
  byte brightness;
  
  tsl.getLuminosity(&reading, &ir);
  Serial.print("Light: ");
  Serial.print(reading);
  Serial.print(", ");

  if (reading <= MIN_LUX) brightness = 0;
  else if (reading >= MAX_LUX) brightness = MAX_BRIGHTNESS;
  else {
    // Percentage in lux range * brightness range + min brightness
    float brightness_percent = (float)(reading - MIN_LUX) / (float)(MAX_LUX - MIN_LUX);
    Serial.print(brightness_percent * 100);
    Serial.print("%, ");
    brightness = (brightness_percent * (MAX_BRIGHTNESS - MIN_BRIGHTNESS)) + MIN_BRIGHTNESS;
  }

  Serial.print(brightness);
  Serial.println(" brightness");
  FastLED.setBrightness(brightness);
  FastLED.show();
}

void loop() {
  setBrightness();

  // Connect to WiFi. We always want a wifi connection for the ESP8266
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.hostname("Weather Clock " + WiFi.macAddress());
    Serial.print("WiFi connecting..");
    WiFi.begin(ssid, pass);

    int c;

    // Wait up to 1 minute for connection...
    for (c = 0; (c < WIFI_TIMEOUT) && (WiFi.status() != WL_CONNECTED); c++) {
      Serial.write('.');
      delay(1000);
    }

    if (c >= WIFI_TIMEOUT) { // If it didn't connect within WIFI_TIMEOUT
      Serial.println("Failed. Will retry...");
      return;
    }
    Serial.println("OK!");
  }

  if (loop_number > (REQUEST_INTERVAL / BRIGHTNESS_INTERVAL) || loop_number == 0) { // Do the new request at certain intervals
    Serial.println("Starting weather request.");
    fill_solid(leds, NUM_LEDS, CRGB::Black); // Set everything to black just in case there is no report

    // get weather for Alameda
    String weather;
    Serial.println("--- Alameda ---");
    weather = getWeatherData(ALAMEDA_URI);
    if (weather[0] != 'E') {
      decodeWeather(weather);
      doLEDs(0); // 0 for Alameda
    }

    // now repeat for SF
    Serial.println("--- San Francisco ---");
    weather = getWeatherData(SF_URI);
    if (weather[0] != 'E') {
      decodeWeather(weather);
      doLEDs(1); // 1 for SF
    }

    FastLED.show();
    loop_number = 0;
  }

  loop_number++;
  delay(BRIGHTNESS_INTERVAL);
}