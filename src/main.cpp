#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <FastLED.h>
#include "GPFont.h"

// Params for width and height
const uint8_t kMatrixWidth = 160;
const uint8_t kMatrixHeight = 8;

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define PIN_LEDS 33

const bool kMatrixSerpentineLayout = true;
String Message = "Przykladowy tekst. Tutaj bedzie prezentacja efektow.";
int xmsg = 160;

const int IR_RECEIVER_PIN = 35;  
const int LIGHT_SENSOR_PIN = 34;  

#define TRIGGER_PIN 32  
#define ECHO_PIN 15    
#define MIN_DISTANCE 20 

#include "Effects.h"
Effects effects;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const char *ap_ssid = "ESP32-Access-Point";
const char *ap_password = "123456789";

const char *sta_ssid = "HH71VM_8689_2.4G";
const char *sta_password = "qJPNTVEf";

bool displayTime = false;
bool displayTextEnabled = true;

WebServer server(80);
WiFiUDP udp;
const int localPort = 8888;
const int UDP_TX_PACKET_MAX_SIZE = 512;

// BME280 sensor
Adafruit_BME280 bme;
#define SDA_PIN 25  // Pin SDA dla I2C
#define SCL_PIN 22  // Pin SCL dla I2C

IRrecv irReceiver(IR_RECEIVER_PIN);
decode_results irResults;

unsigned long lastLightSensorReadTime = 0;
const int lightSensorReadInterval = 1000;

bool autoBrightnessEnabled = true;

void connectToWiFi(const char *ssid, const char *password);
void displayTimeOnMatrix(); 
void handleIRCommand();
void handleLightSensor();
void handleColorWipe();

void drawMesg(String textmsg)
{
  // Modyfikacja funkcji drawMesg, aby uwzględniała jasność
 if (autoBrightnessEnabled) {
    int brightness = analogRead(LIGHT_SENSOR_PIN);  // Odczytaj wartość jasności
    brightness = map(brightness, 0, 1023, 255, 0);  // Zmapuj do zakresu jasności LED

    // Ustaw jasność dla wszystkich pikseli
    FastLED.setBrightness(brightness);
  }

  memset(leds, 0x0, NUM_LEDS * 3);
  int text_length = -(textmsg.length() * 8);
  effects.setCursor(xmsg, 0);
  effects.print(textmsg);
  xmsg -= 1;
  
  if (xmsg < text_length)
  {
    xmsg = 160; // Ustaw ponownie xmsg na początkową pozycję
  }
}

void handleRoot()
{
  String html = "<html><head><style>button {background-color: #4CAF50;color: white;padding: 15px 32px;text-align: center;";
  html += "text-decoration: none;display: inline-block;font-size: 16px;margin: 4px 2px;cursor: pointer;border: none;border-radius: 10px;}";
  html += "body {text-align:center;}</style></head>";
  html += "<body><h1>LED Matrix Text Display</h1>";
  html += "<p>Enter text to display on the LED matrix:</p>";
  html += "<input type='text' id='displayText' placeholder='Enter text'>";
  html += "<input type='color' id='colorPicker' value='#FF0000'>";  // Default color: red
  html += "<button onclick='changeColor()'>Change Color</button>";
  html += "<button onclick='displayTextOnMatrix()'>Display Text</button>";
  html += "<p>Auto Brightness:</p>";
  html += "<label><input type='radio' name='autoBrightness' value='1' onclick='enableAutoBrightness()'> Yes</label>";
  html += "<label><input type='radio' name='autoBrightness' value='0' onclick='disableAutoBrightness()'> No</label>";
  html += "<p>Set Brightness:</p>";
  html += "<button onclick='increaseBrightness()'>Increase Brightness</button>";
  html += "<button onclick='decreaseBrightness()'>Decrease Brightness</button>";  
  html += "<p>Current time: <span id='time'></span></p>";
  html += "<p><span id='temperature'></span></p>";
  html += "<p><span id='humidity'></span></p>";
  html += "<p><span id='pressure'></span></p>";
  html += "<p><button onclick='displayTemperature()'>Display Temperature</button>";
  html += "<button onclick='displayHumidity()'>Display Humidity</button>";
  html += "<button onclick='displayPressure()'>Display Pressure</button></p>";
  html += "<button onclick='handleDisplayTime()'>Display Time</button>";
  html += "<button onclick='handleColorWipe()'>Color Wipe</button>";
  html += "<script>";

  html += "function changeColor() {";
  html += "var colorPicker = document.getElementById('colorPicker');";
  html += "var color = colorPicker.value;";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/changeColor?color=' + color.substr(1), true);";  // Exclude '#' from the color value
  html += "xhr.send();}";

  html += "function displayTextOnMatrix() {";
  html += "var textToDisplay = document.getElementById('displayText').value;";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/displayText?text=' + textToDisplay, true);";
  html += "xhr.send();}";

  html += "function updateTime() {";
  html += "var timeElement = document.getElementById('time');";
  html += "var now = new Date();";
  html += "timeElement.innerHTML = now.toLocaleString();}";
  html += "setInterval(updateTime, 1000);";

  html += "function displayTemperature() {";
  html += "var xhrTemp = new XMLHttpRequest();";
  html += "xhrTemp.open('GET', '/getTemperature', true);";
  html += "xhrTemp.onload = function() {";
  html += "var temperature = xhrTemp.responseText;";
  html += "document.getElementById('temperature').innerHTML = 'Temperature: ' + temperature + ' C';";
  html += "};";
  html += "xhrTemp.send();}";

  html += "function displayHumidity() {";
  html += "var xhrHumidity = new XMLHttpRequest();";
  html += "xhrHumidity.open('GET', '/getHumidity', true);";
  html += "xhrHumidity.onload = function() {";
  html += "var humidity = xhrHumidity.responseText;";
  html += "document.getElementById('humidity').innerHTML = 'Humidity: ' + humidity + ' %';";
  html += "};";
  html += "xhrHumidity.send();}";

  html += "function displayPressure() {";
  html += "var xhrPressure = new XMLHttpRequest();";
  html += "xhrPressure.open('GET', '/getPressure', true);";
  html += "xhrPressure.onload = function() {";
  html += "var pressure = xhrPressure.responseText;";
  html += "document.getElementById('pressure').innerHTML = 'Pressure: ' + pressure + ' hPa';";
  html += "};";
  html += "xhrPressure.send();}";

  html += "function handleDisplayTime() {";
  html += "var xhrTime = new XMLHttpRequest();";
  html += "xhrTime.open('GET', '/displayTime', true);";
  html += "xhrTime.send();}";

  html += "function increaseBrightness() {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/increaseBrightness', true);";
  html += "xhr.send();}";

  html += "function decreaseBrightness() {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/decreaseBrightness', true);";
  html += "xhr.send();}";

  html += "function enableAutoBrightness() {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/setAutoBrightness?value=1', true);";
  html += "xhr.send();}";

  html += "function disableAutoBrightness() {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/setAutoBrightness?value=0', true);";
  html += "xhr.send();}";

  html += "function handleColorWipe() {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/handleColorWipe', true);";
  html += "xhr.send();}";

  html += "setInterval(updateTime, 1000);"; // Aktualizacja czasu co 1000 ms (1 sekunda)
  html += "updateTime();"; // Wywołanie funkcji updateTime przy ładowaniu strony
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

// Globalna zmienna przechowująca aktualny kolor
CRGB currentColor = CRGB::Black;  // Domyślnie ustawiony na czarny

void handleChangeColor()
{
  String colorValue = server.arg("color");
  uint32_t newColor = strtol(colorValue.c_str(), NULL, 16);  // Convert hex string to integer

  // Przypisz nowy kolor
  currentColor.r = (newColor >> 16) & 0xFF;
  currentColor.g = (newColor >> 8) & 0xFF;
  currentColor.b = newColor & 0xFF;

  // Ustaw kolor w efektach
  effects.currentColor = currentColor;

  server.send(200, "text/plain", "Color changed");
  FastLED.show();
}

void handleDisplayText()
{
  String textToDisplay = server.arg("text");
  Serial.print("Received text to display: ");
  Serial.println(textToDisplay);
  Message = textToDisplay; // Aktualizuj zmienną Message z nowym tekstem
  drawMesg(Message);
  server.send(200, "text/plain", "Text displayed on the LED matrix: " + textToDisplay);
}

void handleGetTemperature()
{
  float temperature = bme.readTemperature();
  String tempMessage = "Temperature: " + String(temperature) + " C";
  Message = tempMessage;
  drawMesg(tempMessage);
  server.send(200, "text/plain", String(temperature));
}

void handleGetHumidity()
{
  float humidity = bme.readHumidity();
  String humidityMessage = "Humidity: " + String(humidity, 2) + " %"; // 2 miejsca po przecinku
  Message = humidityMessage;
  drawMesg(humidityMessage);
  server.send(200, "text/plain", String(humidity, 2));
}

void handleGetPressure()
{
  float pressure = bme.readPressure() / 100.0F; // hPa
  String pressureMessage = "Pressure: " + String(pressure, 2) + " hPa"; // 2 miejsca po przecinku
  Message = pressureMessage;
  drawMesg(pressureMessage);
  server.send(200, "text/plain", String(pressure, 2));
}

void handleDisplayTime()
{
  timeClient.update();
  
  // Pobierz godzinę i minutę w formie stringa
  int utcOffset = 1;
  int localHour = (timeClient.getHours() + utcOffset) % 24;
  String timeMessage = "Time: " + String(localHour) + ":" + String(timeClient.getMinutes());

  Serial.println("Time Message: " + timeMessage); // Wiersz do śledzenia w konsoli szeregowej

  Message = timeMessage;
  drawMesg(timeMessage);
  server.send(200, "text/plain", "Displaying Time on Matrix");
}

// Deklaracja zmiennej przechowującej aktualną jasność
int currentBrightness = 30;  // Początkowa jasność, dostosuj według potrzeb

void handleIncreaseBrightness() {
  // Zwiększ jasność o 10 (dostosuj wartość według potrzeb)
  currentBrightness = min(255, currentBrightness + 20);
  FastLED.setBrightness(currentBrightness);
  FastLED.show();
  server.send(200, "text/plain", "Jasność zwiększona");
}

void handleDecreaseBrightness() {
  // Zmniejsz jasność o 10 (dostosuj wartość według potrzeb)
  currentBrightness = max(0, currentBrightness - 20);
  FastLED.setBrightness(currentBrightness);
  FastLED.show();
  server.send(200, "text/plain", "Jasność zmniejszona");
}

void handleSetAutoBrightness() {
  String value = server.arg("value");
  Serial.print("Received value: ");
  Serial.println(value);
  autoBrightnessEnabled = (value.toInt() == 1);
  server.send(200, "text/plain", "Auto Brightness set to " + value);
}

void connectToWiFi(const char *ssid, const char *password)
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address (Station): ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);

  // Initialize BME280 sensor
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bme.begin(0x76))
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      delay(10);
  }

  FastLED.addLeds<WS2812B, PIN_LEDS, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(30);
  effects.setFont(GPFont8x8);

  irReceiver.enableIRIn();  // Inicjalizacja odbiornika IR

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  delay(100);

  Serial.println("Access Point ESP32");
  Serial.print("IP address (Access Point): ");
  Serial.println(WiFi.softAPIP());

  connectToWiFi(sta_ssid, sta_password);

  udp.begin(localPort);
  Serial.println("UDP server started");

  timeClient.begin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/displayText", HTTP_GET, handleDisplayText);
  server.on("/getTemperature", HTTP_GET, handleGetTemperature);
  server.on("/getHumidity", HTTP_GET, handleGetHumidity);
  server.on("/getPressure", HTTP_GET, handleGetPressure);
  server.on("/displayTime", HTTP_GET, handleDisplayTime);
  server.on("/changeColor", HTTP_GET, handleChangeColor);
  server.on("/increaseBrightness", HTTP_GET, handleIncreaseBrightness);
  server.on("/decreaseBrightness", HTTP_GET, handleDecreaseBrightness);
  server.on("/setAutoBrightness", HTTP_GET, handleSetAutoBrightness);
  server.on("/handleColorWipe", HTTP_GET, handleColorWipe);

  server.begin();
}

void loop()
{
  server.handleClient();

  // Zapisz oryginalny tekst przed aktualizacją efektów
  String originalMessage = Message;

  // Aktualizuj efekty LED
  if (displayTime)
  {
    handleDisplayTime();
      Serial.println("Displaying Time on Matrix"); // Dodaj ten wiersz do śledzenia w konsoli szeregowej

    displayTime = false;
  }
  else
  {
    effects.FillNoise();
    drawMesg(Message);
  }

  FastLED.show();

  timeClient.update();

  // Sprawdź, czy tekst został zmieniony
  if (originalMessage != Message && !displayTime)
  {
    EVERY_N_SECONDS(10)
    {
      effects.NoiseVariablesSetup();
    }
  }

  handleIRCommand();

   // Odczytaj wartość czujnika światła tylko raz na sekundę
  unsigned long currentMillis = millis();
  if (currentMillis - lastLightSensorReadTime >= lightSensorReadInterval) {
    handleLightSensor();
    lastLightSensorReadTime = currentMillis;
  }

  delay(10);
}

void handleIRCommand()
{
  if (irReceiver.decode(&irResults))
  {
    Serial.println("Received IR command:");
    Serial.println(irResults.value, HEX);

    // Dodaj debugowanie
    Serial.println("Handling IR command...");

    // Obsługa komendy z pilota IR
    switch (irResults.value)
    {
      case 0x00FF30CF:  // Przycisk 1
        Serial.println("Button 1 pressed");
        // Wyświetl wiadomość
        handleDisplayText();
        break;

      case 0x00FF18E7:  // Przycisk 2
        Serial.println("Button 2 pressed");
        // Wyświetl temperaturę
        handleGetTemperature();
        break;

      case 0x00FF7A85:  // Przycisk 3
        Serial.println("Button 3 pressed");
        // Wyświetl wilgotność
        handleGetHumidity();
        break;

      case 0x00FF10EF:  // Przycisk 4
        Serial.println("Button 4 pressed");
        // Wyświetl ciśnienie
        handleGetPressure();
        break;

      case 0x00FF38C7:  // Przycisk 5 (dodany)
        Serial.println("Button 5 pressed");
        // Dodaj obsługę dla przycisku 5 według potrzeb
        handleDisplayTime();
        break;

      case 0x00FF02FD:  // Przycisk VOL+
        Serial.println("Volume Up pressed");
        currentBrightness = min(255, currentBrightness + 20);
        FastLED.setBrightness(currentBrightness);
        FastLED.show();
        Serial.println("Brightness changed");
      break;

      case 0x00FF9867:  // Przycisk VOL-
        Serial.println("Volume Down pressed");
        currentBrightness = max(0, currentBrightness - 20);
        FastLED.setBrightness(currentBrightness);
        FastLED.show();
        Serial.println("Brightness changed");
        break;

      case 0x00FF5AA5: // Przycisk 6
        Serial.println("Button 6 pressed");
        // Wyświetl ciśnienie
        handleColorWipe();
        break;

      default:
        Serial.println("Unknown button pressed");
        break;
    }

    irReceiver.resume();  // Odblokuj odbiornik IR do odbioru kolejnych sygnałów
  }
}

void handleLightSensor()
{
  if (autoBrightnessEnabled) {
    int lightValue = analogRead(LIGHT_SENSOR_PIN);
    Serial.print("Light Sensor Value: ");
    Serial.println(lightValue);
  }
}

void handleColorWipe() {
  // Clear all LEDs
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(500);

  String color1PickerValue = "00FF00";  // Zielony
  String color2PickerValue = "FF0000";  // Czerwony
  String color3PickerValue = "0000FF";  // Niebieski
  String color4PickerValue = "FFFF00";  // Żółty

  // Check if the color values are not empty
  if (color1PickerValue.length() > 0 && color2PickerValue.length() > 0 &&
      color3PickerValue.length() > 0 && color4PickerValue.length() > 0) {
    uint32_t color1 = strtol(color1PickerValue.c_str(), NULL, 16);
    uint32_t color2 = strtol(color2PickerValue.c_str(), NULL, 16);
    uint32_t color3 = strtol(color3PickerValue.c_str(), NULL, 16);
    uint32_t color4 = strtol(color4PickerValue.c_str(), NULL, 16);

    // Illuminate the first set of 64 LEDs with color1 (zielony)
    for (int i = 0; i < 64; i++) {
      leds[i] = color1;
    }
    FastLED.show();
    delay(1000);  // Adjust the delay as needed

    // Clear the LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);

    // Illuminate the second set of 64 LEDs with color2 (czerwony)
    for (int i = 64; i < 128; i++) {
      leds[i] = color2;
    }
    FastLED.show();
    delay(1000);  // Adjust the delay as needed

    // Clear the LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);

    // Illuminate the third set of 64 LEDs with color3 (niebieski)
    for (int i = 128; i < 192; i++) {
      leds[i] = color3;
    }
    FastLED.show();
    delay(1000);  // Adjust the delay as needed

    // Clear the LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);

    // Illuminate the fourth set of 64 LEDs with color4 (żółty)
    for (int i = 192; i < 256; i++) {
      leds[i] = color4;
    }
    FastLED.show();
    delay(1000);  // Adjust the delay as needed

    // Clear the LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);

    server.send(200, "text/plain", "Color wipe executed");
  }
}

