//#define DEBUG

// Config version
#define CONFIG_VERSION "GL2"

// SyrotaAutomation parameters
#define RS485_CONTROL_PIN 2
#define NET_ADDRESS "GarageSens"

// Pin definitions
#define PIR_NORTH_PIN A0
#define PIR_MIDDLE_PIN A1
#define PIR_SOUTH_PIN A2
#define DOOR_GARAGE_PIN 6
#define DOOR_BACK_PIN 9
#define DOOR_ENTRY_PIN 8
#define DHT_PIN 3
#define LIGHT_OUT_PIN 7
#define GARAGE_RELAY_PIN 5
// Trigger, echo, max distance (CM)
#define ULTRASONIC_NORTH 10, 11, 200
#define ULTRASONIC_SOUTH 12, 13, 200

struct configuration_t {
  char checkVersion[4]; // This is for detection if we have right settings or not
  unsigned long baudRate; // Serial/RS-485 rate: 9600, 14400, 19200, 28800, 38400, 57600, or 
  unsigned long motionLightOnTime; // how many milliseconds to keep the light on after motion is detected
  unsigned long doorLightOnTime; // same as ^ but when door is opened/closed
  unsigned int dhtReadInterval; // how often to poll DHT (milliseconds)
  unsigned int garageButtonDelay; // How long (millis) relay should be kept energized when we press garage door button
  unsigned int sonicReadInterval; // How often to read sonic sensors (milliseconds)
};

struct dht_t {
  unsigned long lastAttemptTime;
  unsigned long lastSuccessTime;
  int temperature;
  int humidity;
};

struct ultrasonic_t {
  unsigned long lastPollTime;
  int distanceNorth;
  int distanceSouth;
};

struct environment_t {
  bool garageDoorOpened; // INPUT_PULLUP inverts reading, so HIGH means it's opened
  bool entryDoorOpened;
  bool backDoorOpened;
  unsigned long lastDoorEventTs; // Last time door was opened or closed
  unsigned long lastMotionTsNorth;
  unsigned long lastMotionTsMiddle;
  unsigned long lastMotionTsSouth;
  unsigned long manualLightOnTs; // when we call on turning the light on manually, this is when the call was made
  unsigned long manualLightOnTime; // How long should the light be on
  unsigned long totalLightOnSeconds; // Track how long light was ON over this boot cycle
  unsigned long lightOnSince; // Track when we switched light on last time
  bool lightStatus;
};

