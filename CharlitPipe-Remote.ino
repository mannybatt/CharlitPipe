//Globals for Wifi Setup and OTA
#include <credentials.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//WiFi Credentials
#ifndef STASSID
#define STASSID "your_ssid"
#endif
#ifndef STAPSK
#define STAPSK  "your_password"
#endif
const char* ssid = STASSID;
const char* password = STAPSK;

//MQTT
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#ifndef AIO_SERVER
#define AIO_SERVER      "your_MQTT_server_address"
#endif
#ifndef AIO_SERVERPORT
#define AIO_SERVERPORT  0000 //Your MQTT port
#endif
#ifndef AIO_USERNAME
#define AIO_USERNAME    "your_MQTT_username"
#endif
#ifndef AIO_KEY
#define AIO_KEY         "your_MQTT_key"
#endif
#define MQTT_KEEP_ALIVE 150
unsigned long previousTime;
float mqttConnectFlag = 0.0;

#define buttonT         D5
#define buttonM         D6
#define buttonB         D7

#define LED_PIN     D8
#define NUM_LEDS    3
#define Pixels    3
#define BRIGHTNESS  255
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define topPixel    0
#define middlePixel 1
#define bottomPixel 2
#define UPDATES_PER_SECOND 100
CRGB leds[Pixels];
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

CRGBPalette16 currentPalette;
TBlendType    currentBlending;

long lastMsg = 0;
int previousState = 0;
unsigned long previousMillis = 0;
unsigned long currentMillis;

//MQTT Startup
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish litButton = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/CharlitPipe_Button");


void setup() {

  FastLED.addLeds<WS2811, 8, GRB>(leds, Pixels);

  //Start Pixels (2 white blinks)
  strip.setBrightness(255);
  strip.begin();
  strip.show();
  setPixel(topPixel, 255, 255, 255);
  setPixel(middlePixel, 255, 255, 255);
  setPixel(bottomPixel, 255, 255, 255);
  strip.show();
  delay(100);
  strip.clear();
  strip.show();
  setPixel(topPixel, 255, 255, 255);
  setPixel(middlePixel, 255, 255, 255);
  setPixel(bottomPixel, 255, 255, 255);
  delay(100);
  strip.clear();
  strip.show();
  delay(500);



  //Establish Connection
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.print("Connected! IP Address: ");
  Serial.println(WiFi.localIP());
  MQTT_connect();

  //Set button pins
  pinMode(buttonT, INPUT_PULLUP);
  pinMode(buttonM, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);



  //Set initial colors (blue,red,purple)
  setDefaultColors();
}

void loop() {

  //Check for Top Button Press
  if (digitalRead(buttonT) == LOW) {
    delay(1);
    litButton.publish(1);
    previousState = 1;
    while (digitalRead(buttonT) == LOW) {
      SingleFadeInOut(topPixel, 0, 0, 255);
      SingleFadeInOut(topPixel, 208, 0, 223);
    }
    setDefaultColors();
  }

  //Check for Middle Button Press
  if (digitalRead(buttonM) == LOW) {
    delay(1);
    litButton.publish(2);
    previousState = 2;
    while (digitalRead(buttonM) == LOW) {
      singleRainbowCycle(3);
    }
    setDefaultColors();
  }

  //Check for Bottom Button Press
  if (digitalRead(buttonB) == LOW) {
    int fiveSecondTimer = 0;
    delay(250);
    //rgbMode change check
    if (digitalRead(buttonB) == LOW && digitalRead(buttonM) == LOW) {
      litButton.publish(4);
      delay(1000);
    }
    else {
      litButton.publish(3);
      previousState = 3;
      fiveSecondTimer = 0;
    }

    while ((digitalRead(buttonB) == LOW) && (fiveSecondTimer <= 1750)) {
      Serial.println(fiveSecondTimer);
      fiveSecondTimer += 250;
      SingleFadeInOut(bottomPixel, 0, 141, 196);
      SingleFadeInOut(bottomPixel, 61, 181, 255);
      delay(100);
    }

    while (digitalRead(buttonB) == LOW) {
      RunningLights(0, 141, 196, 50);
    }
    setDefaultColors();

  }

  //Manage the '0' Publish
  if (previousState != 0) {
    litButton.publish(0);
    previousState = 0;
    currentMillis = 0;
    setDefaultColors();
    delay(100);
  }

  //Ping Timer
  uint8_t secondHand = (millis() / 1000) % 10  /*60*/;
  static uint8_t lastSecond = 99;
  if (lastSecond != secondHand) {
    lastSecond = secondHand;
    if (secondHand == 240 /*4 Mins.*/) {
      if (! mqtt.ping()) {
        mqtt.disconnect();
      }
    }
  }
}


void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    Serial.println("Connected");
    return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      //while (1);
      Serial.println("Wait 10 min to reconnect");
      delay(600000);
    }
  }
  Serial.println("MQTT Connected!");
}

void RunningLights(byte red, byte green, byte blue, int WaveDelay) {
  int Position = 0;

  for (int j = 0; j < NUM_LEDS * 2; j++)
  {
    Position++; // = 0; //Position + Rate;
    for (int i = 0; i < NUM_LEDS; i++) {
      // sine wave, 3 offset waves make a rainbow!
      //float level = sin(i+Position) * 127 + 128;
      //setPixel(i,level,0,0);
      //float level = sin(i+Position) * 127 + 128;
      setPixel(i, ((sin(i + Position) * 127 + 128) / 255)*red,
               ((sin(i + Position) * 127 + 128) / 255)*green,
               ((sin(i + Position) * 127 + 128) / 255)*blue);
    }

    showStrip();
    delay(WaveDelay);
  }
}

void SingleFadeInOut(int pixel, byte red, byte green, byte blue) {
  float r, g, b;

  for (int k = 0; k < 256; k = k + 1) {
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setPixel(pixel, r, g, b);
    showStrip();
    delay(1);
  }

  if (digitalRead(buttonT) == HIGH) {
    return;
  }

  for (int k = 255; k >= 0; k = k - 2) {
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setPixel(pixel, r, g, b);
    showStrip();
    delay(1);
  }
}

void singleRainbowCycle(int SpeedDelay) {
  byte *c;
  uint16_t i, j;

  for (j = 0; j < 256; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < 1; i++) {
      c = Wheel(((i * 256 / NUM_LEDS) + j) & 255);
      setPixel(topPixel, *c, *(c + 1), *(c + 2));
      setPixel(middlePixel, *c, *(c + 1), *(c + 2));
      setPixel(bottomPixel, *c, *(c + 1), *(c + 2));
    }
    showStrip();
    delay(SpeedDelay);
  }
}

byte * Wheel(byte WheelPos) {
  static byte c[3];

  if (WheelPos < 85) {
    c[0] = WheelPos * 3;
    c[1] = 255 - WheelPos * 3;
    c[2] = 0;
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    c[0] = 255 - WheelPos * 3;
    c[1] = 0;
    c[2] = WheelPos * 3;
  } else {
    WheelPos -= 170;
    c[0] = 0;
    c[1] = WheelPos * 3;
    c[2] = 255 - WheelPos * 3;
  }

  return c;
}

void SetupAuroraPalette() {
  CRGB darkPurp = CRGB(141, 0, 196);
  CRGB lightPurp = CRGB(181, 61, 255);
  CRGB lightBlue = CRGB(1, 126, 213);
  CRGB lightGreen = CRGB(0, 234, 141);
  CRGB darkGreen = CRGB(20, 232, 30);

  currentPalette = CRGBPalette16(
                     darkPurp, lightPurp, lightBlue, lightGreen,
                     darkGreen, darkPurp, lightPurp, lightBlue,
                     lightGreen, darkGreen, darkPurp, lightPurp,
                     lightBlue, lightGreen, darkGreen, darkPurp);
}

void FillLEDsFromPaletteColors( uint8_t colorIndex) {
  uint8_t brightness = 255;

  for ( int i = 0; i < Pixels; i++) {
    leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
    colorIndex += 3;
  }
}



void setPixel(int Pixel, byte red, byte green, byte blue) {
#ifdef ADAFRUIT_NEOPIXEL_H
  // NeoPixel
  strip.setPixelColor(Pixel, strip.Color(red, green, blue));
#endif
#ifndef ADAFRUIT_NEOPIXEL_H
  // FastLED
  leds[Pixel].r = red;
  leds[Pixel].g = green;
  leds[Pixel].b = blue;
#endif
}

void setAll(byte red, byte green, byte blue) {
  for (int i = 0; i < NUM_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

void showStrip() {
#ifdef ADAFRUIT_NEOPIXEL_H
  // NeoPixel
  strip.show();
#endif
#ifndef ADAFRUIT_NEOPIXEL_H
  // FastLED
  FastLED.show();
#endif
}

void setDefaultColors() {
  setPixel(topPixel, 0, 0, 255);
  setPixel(middlePixel, 0, 255, 0);
  setPixel(bottomPixel, 0, 141, 196);
  strip.show();
}
