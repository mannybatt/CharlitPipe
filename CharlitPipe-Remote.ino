
// ***************************************
// ********** Global Variables ***********
// ***************************************


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

#define buttonT         5
#define buttonM         6
#define buttonB         7

#define LED_PIN     8
#define NUM_LEDS    3
#define BRIGHTNESS  255
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define topPixel    0
#define middlePixel 1
#define bottomPixel 2
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

long lastMsg = 0;
int status = 0;

//MQTT Startup
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish litButton = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/CharlitPipe_Button");


// ***************************************
// *************** Setup *****************
// ***************************************
void setup() {
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


  setPixel(topPixel, 0, 0, 255);
  setPixel(middlePixel, 0, 255, 0);
  setPixel(bottomPixel, 0, 131, 206);
  strip.show();


  //Set button pins
  delay(1000);
  pinMode(buttonT, INPUT_PULLUP);
  pinMode(buttonM, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);
  //pinMode(pixels, OUTPUT);
}


// ***************************************
// ************* Da Loop *****************
// ***************************************
void loop() {
  long now = millis();

  //Button status is checked constantly however,
  //Publish occurs every ↓ milliseconds with any changes
  if (now - lastMsg > 1050) {
    lastMsg = now;

    //Print a successful publish or failure. Reset status after publish.
    if (! litButton.publish(status)) {
      Serial.println("Failed");
    }
    else {
      Serial.println("OK!");
    }
    status = 0;
  }
  //Grab button states before publish interval
  else {
    if (digitalRead(buttonT) == LOW) {
      status = 1;
    }
    if (digitalRead(buttonM) == LOW) {
      status = 2;
    }
    if (digitalRead(buttonB) == LOW) {
      status = 3;
    }
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


// ***************************************
// ********** Backbone Methods ***********
// ***************************************
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

void FadeInOut(byte red, byte green, byte blue) {
  float r, g, b;

  for (int k = 0; k < 256; k = k + 1) {
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setAll(r, g, b);
    showStrip();
  }

  for (int k = 255; k >= 0; k = k - 2) {
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setAll(r, g, b);
    showStrip();
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

// Set all LEDs to a given color and apply it (visible)
void setAll(byte red, byte green, byte blue) {
  for (int i = 0; i < NUM_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

// Apply LED color changes
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
