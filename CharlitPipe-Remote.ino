



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

//Button Pins
#define buttonT         D5
#define buttonM         D6
#define buttonB         D7

//RGB
#include <Adafruit_NeoPixel.h>
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
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

//MQTT Startup
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish litButton = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/CharlitPipe_Button");
Adafruit_MQTT_Publish guidingKeyPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/GuidingKey_Status");
Adafruit_MQTT_Subscribe rideStop = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/CharlitPipe_RideStop");

//Variables
long lastMsg = 0;
int previousState = 0;
unsigned long previousMillis = 0;
unsigned long currentMillis;
int starsMode = 0;




// ***************************************
// *************** Setup *****************
// ***************************************


void setup() {

  //Initialize Leds and Flash White Lights Twice
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

  //Initialize Buttons
  pinMode(buttonT, INPUT_PULLUP);
  pinMode(buttonM, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);
  if (digitalRead(buttonT) == LOW) {
    starsMode = 1;
    delay(200);
    setPixel(topPixel, 0, 0, 255);
    setPixel(middlePixel, 0, 0, 255);
    setPixel(bottomPixel, 0, 0, 255);
    strip.show();
    delay(70);
    strip.clear();
    strip.show();
  }
  setDefaultColors();

  //Initialize Serial, WiFi, & OTA
  wifiSetup();

  //Initialize MQTT
  mqtt.subscribe(&rideStop);
  MQTT_connect();
}


// ***************************************
// ************* Da Loop *****************
// ***************************************


void loop() {

  //OTA & MQTT
  ArduinoOTA.handle();
  MQTT_connect();

  //Check for Top Button Press, change to FireFlies
  //This is an effort to use this for Hermione on the Stars Lighting Module
  if (starsMode == 1) {   
    if (digitalRead(buttonT) == LOW) {
      delay(400);

      //Lower Twinkle Stars
      if (digitalRead(buttonT) == LOW && digitalRead(buttonB) == LOW) {
        guidingKeyPub.publish(3);
        previousState = 13;
        while (digitalRead(buttonT) == LOW) {
          SingleFadeInOut(topPixel, 0, 170, 255);
          while(digitalRead(buttonB) == LOW){
            guidingKeyPub.publish(3);
            SingleFadeInOut(topPixel, 0, 170, 255);
            SingleFadeInOut(bottomPixel, 0, 255, 0);
            while (digitalRead(buttonT) == LOW && digitalRead(buttonB) == LOW){
              SingleFadeInOut(topPixel, 0, 170, 255);
              SingleFadeInOut(bottomPixel, 0, 255, 0);
            }
          }
        }
      }

      //Raise Twinkle Stars
      if (digitalRead(buttonT) == LOW && digitalRead(buttonM) == LOW) {
        guidingKeyPub.publish(4);
        previousState = 14;
        while (digitalRead(buttonT) == LOW) {
          SingleFadeInOut(topPixel, 0, 170, 255);
          while(digitalRead(buttonM) == LOW){
            guidingKeyPub.publish(3);            
            while (digitalRead(buttonT) == LOW && digitalRead(buttonM) == LOW){
              SingleFadeInOut(topPixel, 0, 170, 255);
              SingleFadeInOut(middlePixel, 255, 0, 0);
            }
          }
        }
      }
      
      //Change to the Twinkle Stars
      else{
      guidingKeyPub.publish(2);
      previousState = 10;
      while (digitalRead(buttonT) == LOW) {
        SingleFadeInOut(topPixel, 0, 170, 255);
      }
      }
      setDefaultColors();
    }
    
    //Check for Middle Button Press, change to WLed
    if (digitalRead(buttonM) == LOW) {
      delay(10);
      guidingKeyPub.publish(1);
      previousState = 11;
      while (digitalRead(buttonM) == LOW) {
        SingleFadeInOut(middlePixel, 220, 0, 255);
      }
      setDefaultColors();
    }

    //Check for Bottom Button Press
    if (digitalRead(buttonB) == LOW) {
      delay(10);
      guidingKeyPub.publish(0);
      previousState = 12;
      while (digitalRead(buttonB) == LOW) {
        SingleFadeInOut(bottomPixel, 255, 0, 40);
      }
      setDefaultColors();
    }
  }
  
  //Charlit Mode
  else {
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
      delay(400);
      //RGBMode change - Check for bottom and middle button press
      if (digitalRead(buttonB) == LOW && digitalRead(buttonM) == LOW) {
        litButton.publish(4);
        setPixel(topPixel, 126, 242, 2);
        setPixel(middlePixel, 126, 242, 2);
        setPixel(bottomPixel, 126, 242, 2);
        strip.show();
        delay(1001);
      }
      //DramaticMode Change - Check for bottom and top button press
      else if (digitalRead(buttonB) == LOW && digitalRead(buttonT) == LOW) {
        litButton.publish(5);
        setPixel(topPixel, 255, 0, 0);
        setPixel(middlePixel, 255, 0, 0);
        setPixel(bottomPixel, 255, 0, 0);
        strip.show();
        delay(1001);
      }
      //Normal Bottom Operation
      else {
        litButton.publish(3);
        previousState = 3;
        fiveSecondTimer = 0;
      }
      //Glow Bottom button while pressed
      while ((digitalRead(buttonB) == LOW) && (fiveSecondTimer <= 1750)) {
        Serial.println(fiveSecondTimer);
        fiveSecondTimer += 250;
        SingleFadeInOut(bottomPixel, 0, 141, 196);
        SingleFadeInOut(bottomPixel, 61, 181, 255);
        delay(100);
      }
      //Start RGB Effect after turbine activates
      while (digitalRead(buttonB) == LOW) {
        RunningLights(0, 141, 196, 50);
      }
      setDefaultColors();
    }

    //Manage the '0' Publish, j
    if (previousState != 0) {
      litButton.publish(0);
      previousState = 0;
      currentMillis = 0;
      setDefaultColors();
      delay(100);
    }
  }

  //Ping Timer
  unsigned long currentTime = millis();
  if ((currentTime - previousTime) > MQTT_KEEP_ALIVE * 1000) {
    previousTime = currentTime;
    if (! mqtt.ping()) {
      mqtt.disconnect();
    }
  }
}




// ***************************************
// ********** Backbone Methods ***********
// ***************************************


//Effect for Turbine
void RunningLights(byte red, byte green, byte blue, int WaveDelay) {
  int Position = 0;

  for (int j = 0; j < NUM_LEDS * 2; j++) {
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

//Effect for 2nd valve
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

//Effect for Drain
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

//Color Wheel (Drain effect)
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

void setPixel(int Pixel, byte red, byte green, byte blue) {
#ifdef ADAFRUIT_NEOPIXEL_H
  // NeoPixe
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
  if (starsMode == 1) {
    setPixel(topPixel, 0, 170, 255);
    setPixel(middlePixel, 220, 0, 255);
    setPixel(bottomPixel, 255, 0, 40);
  }
  else {
    setPixel(topPixel, 0, 0, 255);
    setPixel(middlePixel, 0, 255, 0);
    setPixel(bottomPixel, 0, 141, 196);
  }
  strip.show();
}

void MQTT_connect() {

  int8_t ret;
  // Stop if already connected.
  if (mqtt.connected()) {
    if (mqttConnectFlag == 0) {
      //Serial.println("Connected");
      mqttConnectFlag++;
    }
    return;
  }
  Serial.println("Connecting to MQTT... ");
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
      Serial.println("Wait 5 secomds to reconnect");
      delay(5000);
    }
  }
}

void wifiSetup() {

  //Serial
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println();
  Serial.println("****************************************");
  Serial.println("Booting");

  //WiFi and OTA
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname("CharlitPipe-Remote");                                                          /** TODO **/
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
