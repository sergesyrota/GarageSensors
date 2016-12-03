#include <EEPROMex.h>
#include <SyrotaAutomation1.h>
#include <DHT.h>
#include <NewPing.h>
#include "config.h"

//
// Hardware configuration
//

SyrotaAutomation net = SyrotaAutomation(RS485_CONTROL_PIN);
DHT dht;
NewPing ultrasonicNorth(ULTRASONIC_NORTH);
NewPing ultrasonicSouth(ULTRASONIC_SOUTH);
dht_t dhtData;
ultrasonic_t ultrasonicData;
environment_t envData;

struct configuration_t conf = {
  CONFIG_VERSION,
  // Default values for config
  9600UL, //unsigned long baudRate; // Serial/RS-485 rate: 9600, 14400, 19200, 28800, 38400, 57600, or 
  1000UL, //unsigned long motionLightOnTime; // how many milliseconds to keep the light on after motion is detected
  2000UL, //unsigned long doorLightOnTime; // same as ^ but when door is opened/closed
  10000, //unsigned int dhtReadInterval; // how often to poll DHT (milliseconds)
  250, //unsigned int garageButtonDelay; // How long (millis) relay should be kept energized when we press garage door button
  10000UL, //unsigned int sonicReadInterval; // How often to read sonic sensors (milliseconds)
};

void setup()
{
  pinMode(PIR_NORTH_PIN, INPUT);
  pinMode(PIR_MIDDLE_PIN, INPUT);
  pinMode(PIR_SOUTH_PIN, INPUT);
  pinMode(DOOR_GARAGE_PIN, INPUT_PULLUP);
  pinMode(DOOR_BACK_PIN, INPUT_PULLUP);
  pinMode(DOOR_ENTRY_PIN, INPUT_PULLUP);
  pinMode(DHT_PIN, INPUT);
  pinMode(LIGHT_OUT_PIN, OUTPUT);
  pinMode(GARAGE_RELAY_PIN, OUTPUT);
  
  dht.setup(DHT_PIN, DHT::DHT11);
  
  readConfig();
  // Set device ID
  strcpy(net.deviceID, NET_ADDRESS);
  Serial.begin(conf.baudRate);
}


void readConfig()
{
  // Check to make sure config values are real, by looking at first 3 bytes
  if (EEPROM.read(0) == CONFIG_VERSION[0] &&
    EEPROM.read(1) == CONFIG_VERSION[1] &&
    EEPROM.read(2) == CONFIG_VERSION[2]) {
    EEPROM.readBlock(0, conf);
  } else {
    // Configuration is invalid, so let's write default to memory
    saveConfig();
  }
}

void saveConfig()
{
  EEPROM.writeBlock(0, conf);
}

void loop()
{
  if (net.messageReceived()) {
    char buf[100];
    if (net.assertCommand("getDht")) {
      sprintf(buf, "{\"t\":%d,\"unit\":\"C\",\"rh\":%d,\"age\":%d}", dhtData.temperature, dhtData.humidity, (millis() - dhtData.lastSuccessTime)/1000);
      net.sendResponse(buf);
    } else if (net.assertCommand("getMotion")) {
      unsigned long mil = millis();
      sprintf(buf, "{\"secondsSinceMotion\":{\"north\": %lu,\"mid\": %lu,\"south\": %lu}}", 
        (mil - envData.lastMotionTsNorth)/1000,
        (mil - envData.lastMotionTsMiddle)/1000,
        (mil - envData.lastMotionTsSouth)/1000
      );
      net.sendResponse(buf);
    } else if (net.assertCommand("getDoors")) {
      sprintf(buf, "{\"garage\":%d,\"entry\":%d,\"back\":%d}", 
        envData.garageDoorOpened,
        envData.entryDoorOpened,
        envData.backDoorOpened);
      net.sendResponse(buf);
    } else if (net.assertCommand("getUltrasonic")) {
      sprintf(buf, "{\"north\":%d,\"south\":%d,\"age\":%d}", 
        ultrasonicData.distanceNorth,
        ultrasonicData.distanceSouth,
        (millis() - ultrasonicData.lastPollTime)/1000);
      net.sendResponse(buf);
    } else if (net.assertCommand("getLight")) {
      sprintf(buf, "{\"status\":%d,\"totalOnSeconds\":%lu,\"uptime\":%lu}", envData.lightStatus, getLightOnSeconds(), (millis()/1000));
      net.sendResponse(buf);
    } else if (net.assertCommand("openGarage")) {
      net.sendResponse("OK");
      if (!envData.garageDoorOpened) {
        pressGarageButton();
      }
    } else if (net.assertCommand("closeGarage")) {
      net.sendResponse("OK");
      if (envData.garageDoorOpened) {
        pressGarageButton();
      }
    } else if (net.assertCommand("lightsOff")) {
      net.sendResponse("OK");
      envData.manualLightOnTs=0;
      envData.manualLightOnTime=0;
      checkLightStatus();
    } else if (net.assertCommandStarts("lightsOn:", buf)) {
      unsigned long tmp = strtol(buf, NULL, 10);
      if (tmp > 0 && tmp < 36000) {
        envData.manualLightOnTime = tmp*1000UL;
        envData.manualLightOnTs=millis();
        net.sendResponse("OK");
      } else {
        net.sendResponse("ERROR");
      }
    } else if (net.assertCommandStarts("set", buf)) {
      processSetCommands();
    } else {
      net.sendResponse("Unrecognized command");
    }
  }
  
  // Check DHT sensor
  if (millis() - dhtData.lastAttemptTime > conf.dhtReadInterval) {
    updateDht();
  }
  
  // Check sonic sensors
  if (millis() - ultrasonicData.lastPollTime > conf.sonicReadInterval) {
    updateUltrasonic();
  }

  // Motion and door sensors
  checkEnvironment();
  // See if we need to do anything with lights
  checkLightStatus();
}

// Check all environment sensors for events
void checkEnvironment()
{
  checkMotion(PIR_NORTH_PIN, envData.lastMotionTsNorth);
  checkMotion(PIR_MIDDLE_PIN, envData.lastMotionTsMiddle);
  checkMotion(PIR_SOUTH_PIN, envData.lastMotionTsSouth);
  
  checkDoor(DOOR_GARAGE_PIN, envData.garageDoorOpened);
  checkDoor(DOOR_BACK_PIN, envData.backDoorOpened);
  checkDoor(DOOR_ENTRY_PIN, envData.entryDoorOpened);
}

// Updates last motion timestamp, if there was motion, and checks the light
void checkMotion(int pin, unsigned long &lastTs) {
  if (digitalRead(pin) == HIGH) {
    lastTs = millis();
    checkLightStatus();
#ifdef DEBUG
    Serial.print("One of the motion sensors is on: ");
    Serial.println(pin);
#endif
  }
}

// takes current reading, makes a decision on event timestamp, and saves reading for future reference
// PULLUP is enabled, so LOW is on, HIGH is off
void checkDoor(int pin, bool &prevState) {
  bool reading = digitalRead(pin);
  if (reading == LOW && prevState==HIGH) {
#ifdef DEBUG
    Serial.println("One of the doors was just opened: ");
    Serial.println(pin);
#endif
    envData.lastDoorEventTs = millis();
    checkLightStatus();
  }
  prevState = reading;
}

// Checks if light should be on or off, and does so (also tracks usage)
void checkLightStatus()
{
  int desiredLight = LOW;
  // Check if motion sensor time was called
  unsigned long NorthMiddle = max(envData.lastMotionTsNorth, envData.lastMotionTsMiddle);
  unsigned long lastMotion = max(NorthMiddle, envData.lastMotionTsSouth);
  if ((millis() - lastMotion) < conf.motionLightOnTime) {
    desiredLight = HIGH;
  }
  
  // Check if door sensor time was called
  if ((millis() - envData.lastDoorEventTs) < conf.motionLightOnTime) {
    desiredLight = HIGH;
  }
  
  // Check if manual light on was called
  if (envData.manualLightOnTs != 0) {
    if ((millis() - envData.manualLightOnTs) < envData.manualLightOnTime) {
      desiredLight = HIGH;
    } else {
      // Cleanup manual timers just in case
      envData.manualLightOnTs = 0;
    }  
  }
  
  // Apply desired light status, and track usage
  if (envData.lightStatus != desiredLight) {
    if (desiredLight == HIGH) {
      envData.lightOnSince = millis();
    } else {
      // Add number of seconds since light was turned on
      envData.totalLightOnSeconds += (millis() - envData.lightOnSince)/1000;
    }
  }
  envData.lightStatus = desiredLight;
  digitalWrite(LIGHT_OUT_PIN, envData.lightStatus);
}

// Gets how long lights were on for this boot cycle, taking into account current light status
unsigned long getLightOnSeconds() {
  unsigned long onTime = envData.totalLightOnSeconds;
  // Add elapsed seconds since light was on, but still not counted in the envData
  if (envData.lightStatus == HIGH) {
    onTime += (millis() - envData.lightOnSince)/1000;
  }
  return onTime;
}

void pressGarageButton()
{
  digitalWrite(GARAGE_RELAY_PIN, HIGH);
  delay(conf.garageButtonDelay);
  digitalWrite(GARAGE_RELAY_PIN, LOW);
}

// Reads DHT sensor, and updates data in the variable
void updateDht()
{
  // Throw out result to make sure we don't have an error
  dht.getTemperature();
  dhtData.lastAttemptTime = millis();
  if (dht.getStatus() == dht.ERROR_NONE) {
    dhtData.lastSuccessTime = millis();
    dhtData.temperature = dht.getTemperature();
    dhtData.humidity = dht.getHumidity();
  }
}

// Reads all ultrasonic sensors and updates poll time
void updateUltrasonic() 
{
  ultrasonicData.distanceNorth = ultrasonicNorth.ping_cm();
  ultrasonicData.distanceSouth = ultrasonicSouth.ping_cm();
  ultrasonicData.lastPollTime = millis();
}

// Write to the configuration when we receive new parameters
void processSetCommands()
{
  char buf[100];
  if (net.assertCommandStarts("setBaudRate:", buf)) {
    long tmp = strtol(buf, NULL, 10);
    // Supported: 9600, 14400, 19200, 28800, 38400, 57600, or 115200
    if (tmp == 9600 ||
      tmp == 14400 ||
      tmp == 19200 ||
      tmp == 28800 ||
      tmp == 38400 ||
      tmp == 57600 ||
      tmp == 115200
    ) {
      conf.baudRate = tmp;
      saveConfig();
      net.sendResponse("OK");
      Serial.end();
      Serial.begin(tmp);
    } else {
      net.sendResponse("ERROR");
    }
  } else if (net.assertCommandStarts("setMotionLightOnSec:", buf)) {
    unsigned int tmp = strtol(buf, NULL, 10);
    if (tmp > 0 && tmp < 3600) {
      conf.motionLightOnTime = tmp*1000;
      saveConfig();
      net.sendResponse("OK");
    } else {
      net.sendResponse("ERROR");
    }
  } else if (net.assertCommandStarts("setDoorLightOnSec:", buf)) {
    unsigned int tmp = strtol(buf, NULL, 10);
    if (tmp > 0 && tmp < 3600) {
      conf.doorLightOnTime = tmp*1000;
      saveConfig();
      net.sendResponse("OK");
    } else {
      net.sendResponse("ERROR");
    }
  } else if (net.assertCommandStarts("setDhtReadSec:", buf)) {
    unsigned int tmp = strtol(buf, NULL, 10);
    if (tmp > 0 && tmp < 3600) {
      conf.dhtReadInterval = tmp*1000;
      saveConfig();
      net.sendResponse("OK");
    } else {
      net.sendResponse("ERROR");
    }
  } else if (net.assertCommandStarts("setButtonDelay:", buf)) {
    unsigned int tmp = strtol(buf, NULL, 10);
    if (tmp > 0 && tmp < 5000) {
      conf.garageButtonDelay = tmp;
      saveConfig();
      net.sendResponse("OK");
    } else {
      net.sendResponse("ERROR");
    }
  } else if (net.assertCommandStarts("setSonicReadInterval:", buf)) {
    unsigned int tmp = strtol(buf, NULL, 10);
    if (tmp > 0 && tmp < 3600000UL) {
      conf.sonicReadInterval = tmp;
      saveConfig();
      net.sendResponse("OK");
    } else {
      net.sendResponse("ERROR");
    }
  } else {
    net.sendResponse("Unrecognized");
  }
}

