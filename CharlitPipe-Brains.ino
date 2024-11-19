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

//Initialize and Subscribe to MQTT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe cp_button = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/CharlitPipe_Button");
Adafruit_MQTT_Subscribe cp_rgbMode = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/CharlitPipe_RGBMode");

//Globals for Relays
#define r_fill D0
#define r_drain D1
#define r_airLock D2
#define r_blow D3
#define r_ledGnd D4
#define r_ledDat D5
#define r_ledVcc D6

//Globals for Light Effects
#include "FastLED.h"
#define NUM_LEDS 78
CRGB leds[NUM_LEDS];
#define PIN 8
int brightValue = 255;
float redStates[NUM_LEDS];
float blueStates[NUM_LEDS];
float greenStates[NUM_LEDS];
float fadeRate = 0.96;

//Globals for System
int theStateofYou = 0; 
uint16_t lastValue = 0;



// ***************************************
// *************** Setup *****************
// ***************************************
void setup() {

  //Initialize Serial 
  Serial.begin(115200);
  Serial.println("Booting");

  //Initialize WiFi & OTA
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
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

  //Initialize MQTT
  mqtt.subscribe(&cp_button);
  mqtt.subscribe(&cp_rgbMode);

  //Initialize Relays
  delay(1000);
  pinMode(r_fill, OUTPUT);
  pinMode(r_drain, OUTPUT);
  pinMode(r_airLock, OUTPUT);
  pinMode(r_blow, OUTPUT);
  pinMode(r_ledGnd, OUTPUT);
  pinMode(r_ledDat, OUTPUT);
  pinMode(r_ledVcc, OUTPUT);
  delay(100);
  digitalWrite(r_fill, LOW);
  digitalWrite(r_drain, LOW);
  digitalWrite(r_airLock, LOW);
  digitalWrite(r_blow, LOW);
  digitalWrite(r_ledGnd, HIGH);
  digitalWrite(r_ledDat, HIGH);
  digitalWrite(r_ledVcc, HIGH);

  //Initialize RGB
  FastLED.addLeds<WS2811, PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
}



// ***************************************
// ************* Da Loop *****************
// ***************************************
void loop() {

  //OTA code
  ArduinoOTA.handle();

  //MQTT Code
  MQTT_connect();


  digitalWrite(r_fill, HIGH);
  delay(250);
  digitalWrite(r_fill, LOW);
  delay(250);
  digitalWrite(r_drain, HIGH);
  delay(250);
  digitalWrite(r_drain, LOW);
  delay(250);
  digitalWrite(r_airLock, HIGH);
  delay(250);
  digitalWrite(r_blow, HIGH);
  delay(500);
  digitalWrite(r_blow, LOW);
  digitalWrite(r_airLock, LOW);
  delay(250);/*
  digitalWrite(r_ledGnd, HIGH);
  delay(250);
  digitalWrite(r_ledGnd, LOW);
  delay(250);
  digitalWrite(r_ledDat, HIGH);
  delay(250);
  digitalWrite(r_ledDat, LOW);
  delay(250);
  digitalWrite(r_ledVcc, HIGH);
  delay(250);
  digitalWrite(r_ledVcc, LOW);
  delay(250);*/

  /*
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(1000))) {
    Serial.println("Subscription Recieved");
    uint16_t value = atoi((char *)cp_button.lastread);    
    
    //Changes the state
    if(value == 0){
        digitalWrite(relay_fill, HIGH);
        digitalWrite(relay_drain, HIGH);
        digitalWrite(relay_airLock, HIGH);
        digitalWrite(relay_blow, HIGH);
        Serial.println("   Metronome");
    }

    if(value == 1){
      digitalWrite(relay_fill, LOW);
      digitalWrite(relay_drain, HIGH);
      digitalWrite(relay_airLock, HIGH);
      digitalWrite(relay_blow, HIGH);
      Serial.println("   Drizzle");
    }

    if(value == 2){
      digitalWrite(relay_fill, HIGH);
      digitalWrite(relay_drain, LOW);
      digitalWrite(relay_airLock, HIGH);
      digitalWrite(relay_blow, HIGH);
      Serial.println("   Hydro Blast");
    }

    if(value == 3){
      if(lastValue != value){
        digitalWrite(relay_fill, HIGH);
        digitalWrite(relay_drain, HIGH);
        digitalWrite(relay_blow, HIGH);
        digitalWrite(relay_airLock, LOW);
        Serial.println("   Aeroblast[Valve]");
        delay(5000);
      }
      else{
        digitalWrite(relay_fill, HIGH);
        digitalWrite(relay_drain, HIGH);
        digitalWrite(relay_blow, LOW);
        digitalWrite(relay_airLock, LOW);
        Serial.println("   Aeroblast[Turbine]");
      }
    }
    lastValue = value;
  } */
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
      Serial.println("Wait 5 secomds to reconnect");
      delay(5000);
    }
  }
  Serial.println("MQTT Connected!");
}
