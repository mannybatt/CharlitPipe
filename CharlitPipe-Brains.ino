

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
Adafruit_MQTT_Publish rideStopFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/CharlitPipe_RideStop");

//Globals for Relays
#define r_fill D0
#define r_drain D1
#define r_airLock D2
#define r_blow D3
#define r_ledGnd D4
#define r_ledDat D5
#define r_ledVcc D6
#define r_3v3 D7

//Globals for System
int turbineBit = 0; //Determines the usage of the Turbine
int dramaticMode = 0; // Determines the change to use Dramatic Lighting
int rgbMode = 0; /*1=PixelBlaze,0=SP108E*/
const unsigned long period = 38890; //This is the Maximum Fill time in ms, 38.89s
const unsigned long heatPeriod = 180000; //This is the Maximum alloted Drain/Drain2 time in ms, 3m
int protectionBarrier = 0; //Determines the monitoring of the Overfill protection
int heatBarrier = 0; //Determines the monitoring of the Overheat protection
int quit = 0;
float mqttConnectFlag = 0.0;
unsigned long startMillis;
unsigned long currentMillis;
unsigned long startMillisFill;
unsigned long currentMillisFill;
unsigned long startMillisDrain;
unsigned long currentMillisDrain;
int currentState = -1;
int lastState = 0;
int faultReset = 0;
int lastT = 0;



// ***************************************
// *************** Setup *****************
// ***************************************

void setup() {

  //Initialize Serial
  Serial.begin(115200);
  Serial.println("Booting");

  //Initialize WiFi & OTA
  wifiSetup();

  //Initialize MQTT
  mqtt.subscribe(&cp_button);

  //Initialize Relays
  delay(1000);
  pinMode(r_fill, OUTPUT);
  pinMode(r_drain, OUTPUT);
  pinMode(r_airLock, OUTPUT);
  pinMode(r_blow, OUTPUT);
  pinMode(r_ledGnd, OUTPUT);
  pinMode(r_ledDat, OUTPUT);
  pinMode(r_ledVcc, OUTPUT);
  pinMode(r_3v3, OUTPUT);
  delay(100);
  digitalWrite(r_fill, LOW);
  digitalWrite(r_drain, LOW);
  digitalWrite(r_airLock, LOW);
  digitalWrite(r_blow, LOW);
  digitalWrite(r_ledGnd, LOW);
  digitalWrite(r_ledDat, LOW);
  digitalWrite(r_ledVcc, LOW);
  delay(250);
  digitalWrite(r_3v3, HIGH);

  delay(1000);
  digitalWrite(r_ledDat, HIGH);
  startMillis = millis();

  Serial.println("***** Charlit Pipe is Ready! *****");
}



// ***************************************
// ************* Da Loop *****************
// ***************************************

void loop() {

  //OTA & MQTT
  ArduinoOTA.handle();
  MQTT_connect();

  //Clear All Faults
  if (faultReset == 0) {
    faultReset++;
    rideStopFeed.publish("-");
  }

  //Prevent [Overfilling] by timing out relays
  if (protectionBarrier > 0) {
    if (protectionBarrier == 2) {
      Serial.println("**Fill Guardian**");
      startMillis = millis();
      protectionBarrier = 1;
    }
    if (protectionBarrier == 1) {
      currentMillis = millis();
      int t = (currentMillis - startMillis) / 1000;
      if (lastT != t) {
        Serial.print(t);
        Serial.print(" ");
      }

      if ((currentMillis - startMillis >= period)) { //test whether the maximum fill period has elapsed
        rideStop();
        protectionBarrier = 0;
        startMillis = currentMillis;
      }
      lastT = t;
    }
  }
  //Prevent [Overheating] by timing out relays
  if (heatBarrier > 0) {
    if (heatBarrier == 2) {
      Serial.println("**Heat Guardian**");
      startMillis = millis();
      heatBarrier = 1;
    }
    if (heatBarrier == 1) {
      currentMillis = millis();
      int t = (currentMillis - startMillis) / 1000;
      if (lastT != t) {
        Serial.print(t);
        Serial.print(" ");
      }
      if ((currentMillis - startMillis >= heatPeriod)) { //test whether the maximum drain period has elapsed
        rideStop();
        heatBarrier = 0;
        startMillis = currentMillis;
      }
      lastT = t;
    }
  }


  //State Manager
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(0.01 /*1000*/))) {
    uint16_t value = atoi((char *)cp_button.lastread);

    // [Standby]
    if (value == 0) {
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_airLock, LOW);
      digitalWrite(r_blow, LOW);
      //This shifts rgb mode during dramaticMode After drain
      if (dramaticMode == 1) {
        digitalWrite(r_ledDat, HIGH);
      }
      protectionBarrier = 0;
      heatBarrier = 0;
      quit = 0;
      lastState = currentState;
      currentState = 0;
      Serial.println("[Standby]");
    }

    // [Fill]
    if (value == 1) {
      if (lastState != 1) {
        protectionBarrier = 2;
        Serial.println("[↑] Fill");
      }
      digitalWrite(r_fill, HIGH);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_airLock, LOW);
      digitalWrite(r_blow, LOW);
      lastState = currentState;
      currentState = 1;
    }

    // [Auto-Fill]
    if (value == 6) {
      Serial.println("[↑↑] Fill-Auto");
      startMillisFill = millis();
      currentMillisFill = millis();
      quit = 0;
      int t = 0;

      while (((currentMillisFill - startMillisFill) <= 38500) && (quit == 0)) { //38.5 seconds
        //Start filling
        digitalWrite(r_fill, HIGH);
        digitalWrite(r_drain, LOW);
        digitalWrite(r_airLock, LOW);
        digitalWrite(r_blow, LOW);

        //Communication
        t = ((currentMillisFill - startMillisFill) / 1000);
        if (lastT != t) {
          Serial.print(t);
          Serial.print(" ");
        }

        //E-Stop Implementation
        Adafruit_MQTT_Subscribe *subscription;
        while ((subscription = mqtt.readSubscription(100))) {
          uint16_t value = atoi((char *)cp_button.lastread);
          if (value > 0) {
            Serial.println("********* E-STOP");
            digitalWrite(r_fill, LOW);
            digitalWrite(r_drain, LOW);
            digitalWrite(r_airLock, LOW);
            digitalWrite(r_blow, LOW);
            delay(250);
          }
        }
        lastT = t;
        currentMillisFill = millis();
      }

      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_airLock, LOW);
      digitalWrite(r_blow, LOW);
      Serial.println("\nFilling Ceased");
      lastState = currentState;
      currentState = 6;
    }

    // [Drain]
    if (value == 2) {
      if (lastState != value) {
        heatBarrier = 2;
        Serial.println("[↓] Drain");
      }
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, HIGH);
      digitalWrite(r_airLock, LOW);
      digitalWrite(r_blow, LOW);
      //This shifts rgb mode during dramaticMode During drain
      if (dramaticMode == 1) {
        //digitalWrite(r_ledVcc, LOW);
        //digitalWrite(r_ledGnd, LOW);
        digitalWrite(r_ledDat, LOW);
      }
      lastState = currentState;
      currentState = 2;
    }

    // [Auto-Drain]
    if (value == 7) {
      Serial.println("[↓↓] Drain-Auto");
      startMillisDrain = millis();
      currentMillisDrain = millis();
      quit = 0;
      int t = 0;

      if (dramaticMode == 1) {
        digitalWrite(r_ledVcc, LOW);
        digitalWrite(r_ledGnd, LOW);
        digitalWrite(r_ledDat, LOW);
      }

      while (((currentMillisDrain - startMillisDrain) <= 90000) && (quit == 0)) { //90 seconds
        //Start filling
        digitalWrite(r_fill, LOW);
        digitalWrite(r_drain, HIGH);
        digitalWrite(r_airLock, LOW);
        digitalWrite(r_blow, LOW);

        //Communication
        t = ((currentMillisDrain - startMillisDrain) / 1000);
        if (lastT != t) {
          Serial.print(t);
          Serial.print(" ");
        }

        //E-Stop Implementation
        Adafruit_MQTT_Subscribe *subscription;
        while ((subscription = mqtt.readSubscription(100))) {
          uint16_t value = atoi((char *)cp_button.lastread);
          if (value > 0) {
            Serial.println("********* E-STOP");
            digitalWrite(r_fill, LOW);
            digitalWrite(r_drain, HIGH);
            digitalWrite(r_airLock, LOW);
            digitalWrite(r_blow, LOW);
            quit = 1;
          }
        }
        lastT = t;
        currentMillisDrain = millis();
      }

      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_airLock, LOW);
      digitalWrite(r_blow, LOW);
      Serial.println("\nDraining Ceased");

      lastState = currentState;
      currentState = 7;

    }

    // [2nd Drain]
    if (value == 3) {
      heatBarrier = 2;
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_blow, LOW);
      digitalWrite(r_airLock, HIGH);
      lastState = currentState;
      currentState = 3;
      Serial.println("[↕] Drain2");
      delay(5000);
      turbineBit = 1;
    }
    // [Turbine]
    if (turbineBit == 1) {
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_blow, HIGH);
      digitalWrite(r_airLock, HIGH);
      turbineBit = 0;
      if (lastState != value) {
        Serial.println("  [↨] Turbine");
      }
    }

    // [Turbine TEST]
    if (value == 33) {
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_blow, HIGH);
      digitalWrite(r_airLock, LOW);
      if (lastState != value) {
        Serial.println("  [↨] Turbine TEST");
      }
    }

    // [Auto 2nd Drain & Turbine]
    if (value == 8) {
      heatBarrier = 2;
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_blow, LOW);
      digitalWrite(r_airLock, HIGH);
      Serial.println("[↕↕] Drain2-Auto");
      delay(5000); //5 seconds
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_blow, HIGH);
      digitalWrite(r_airLock, HIGH);
      delay(75000); //75 seconds
      lastState = currentState;
      currentState = 8;
      Serial.println("  [↨↨] Turbine-Auto");
      digitalWrite(r_fill, LOW);
      digitalWrite(r_drain, LOW);
      digitalWrite(r_airLock, LOW);
      digitalWrite(r_blow, LOW);
    }

    // [RGB Mode Toggle]
    if (value == 4) {
      if (rgbMode == 0) { /*Change to PixelBlaze*/
        Serial.println("Entering PixelBlaze Mode");
        digitalWrite(r_ledVcc, LOW);
        digitalWrite(r_ledGnd, LOW);
        digitalWrite(r_ledDat, LOW);
        rgbMode = 1;
      }
      else if (rgbMode == 1) { /*Change to WLED*/
        Serial.println("Entering SP108E Mode");
        //digitalWrite(r_ledVcc, HIGH);
        //digitalWrite(r_ledGnd, HIGH);
        digitalWrite(r_ledDat, HIGH);
        rgbMode = 0;
      }
      value = 0;
    }

    // [Dramatic Mode Toggle]
    if (value == 5) {
      if (dramaticMode == 0) {
        Serial.println("Dramatic Mode Enabled");
        dramaticMode = 1;
      }
      else if (dramaticMode == 1) {
        Serial.println("Dramatic Mode Disabled");
        dramaticMode = 0;
      }
    }
  }
  delay(1);
}



// ***************************************
// ********** Backbone Methods ***********
// ***************************************

void rideStop() {

  Serial.println();
  Serial.println("■ RIDE STOPPED ■");
  rideStopFeed.publish("■ RIDE STOPPED ■");
  digitalWrite(r_fill, LOW);
  digitalWrite(r_drain, LOW);
  digitalWrite(r_airLock, LOW);
  digitalWrite(r_blow, LOW);
  delay(500);
  digitalWrite(r_fill, LOW);
  digitalWrite(r_drain, LOW);
  digitalWrite(r_airLock, LOW);
  digitalWrite(r_blow, LOW);
  delay(500);
  digitalWrite(r_fill, LOW);
  digitalWrite(r_drain, LOW);
  digitalWrite(r_airLock, LOW);
  digitalWrite(r_blow, LOW);
  currentState = 0;
  lastState = 0;
  while (faultReset == 1) {
    delay(3000);
    faultReset = 1;
  }
  digitalWrite(r_drain, HIGH);
  delay(250);
  digitalWrite(r_drain, LOW);
  delay(250);
  digitalWrite(r_drain, HIGH);
  delay(250);
  digitalWrite(r_drain, LOW);
}


void wifiSetup() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname("CharlitBrains");
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

void MQTT_connect() {

  int8_t ret;
  // Stop if already connected.
  if (mqtt.connected()) {
    if (mqttConnectFlag == 0) {
      Serial.println("Connected");
      mqttConnectFlag++;
    }
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
}
