/*
  Sound to light system for installation Suuret Muinaiset in Turku, Finland.
  Mono audio playback outputs audio RMS or peak values to PWM output, driving LED strip through a MOSFET.
  3 players in action: 1 leader (LO short for LONG) has an RTC module and control 2 followers through Serial3 (SM for SMALL and SS for SEASHELL)
  The whole system can be controlled via USB serial commands or messages sent from the leader to the followers.
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <elapsedMillis.h>
#include <RTClib.h>
#include "LedzCtrl.h"       //custom lib for LEDs array control
#include "mySysCtrl.h"      //custom lib for system control
//#include <Watchdog_t4.h>

//OBJECTS
//audio
AudioPlaySdWav wavPlayer;
AudioAnalyzePeak audioPeak;
AudioAnalyzeRMS audioRMS;
AudioOutputI2S audioOutput;
AudioControlSGTL5000 sgtl5000;
//RTC
RTC_DS3231 rtc;
//watchdog
//WDT_T4<WDT1> wdt;

//AUDIO MATRIX
AudioConnection patchCord1(wavPlayer, 0, audioOutput, 0);
AudioConnection patchCord2(wavPlayer, 0, audioPeak, 0);
AudioConnection patchCord3(wavPlayer, 0, audioRMS, 0);

//SD CARD
const int SDCARD_CS_PIN = 10;
const int SDCARD_MOSI_PIN = 11;  //teensy uses 11 instead of 7
const int SDCARD_SCK_PIN = 13;   //teensy uses 13 instead of 14

//DIGITAL PINS
const int REL_1 = 17;
const int REL_2 = 16;
const int LED_1 = 2;
const int LED_2 = 3;
const int LED_3 = 4;
const int LED_4 = 5;
const int PWM_PIN = 6;
const int SMALL_PIN = 30;
const int SEASHELL_PIN = 28;
const int LONG_PIN = 32;

//ANALOG PINS
const uint8_t VOL_CTRL_PIN = A8; //pin22

//IO ARRAYS
const uint8_t LED_ARRAY[4] = {LED_1, LED_2, LED_3, LED_4};

//SYSTEM
/* -----------------------
* VARIABLES YOU CAN CHANGE
*/ -----------------------
float audioVolume = 0.8;  //any float between 0.0 and 1.0. Will be automatic startup volume if the knobCtrl is set to false.
bool knobCtrl = false;     //true to activate the volume knob control at startup, false to maintain audioVolume at startup. Can be switched later in the serial monitor using the command'K'
const int START_HOUR = 8;  //daily wake-up time
const int END_HOUR = 20;   //daily sleep time
/* -----------------------
* ########################
*/ -----------------------

int rangePWM = 255;       //0-255, controls brightness
int currentCode = 0;      //starts at 0
const int STARTUP_DELAY = 10000;   //inactivity time after setup for LONG player
int trackIteration = 0;    //resets every day at START_HOUR
int pwmFreq = 25;   //Hz, refresh rate for the PWM
const int REL_SW_DELAY = 500; //delay in between the relays are being switched
bool systemAwake = false;  //activity time between START_HOUR and END_HOUR
bool playbackStatus = false;  //if the player is currently playing back
bool messageIncoming = true; //if a mesage is currently coming in

const int MSG_BUFFER_SIZE = 512;  //how long can a message be
char messageBuffer[MSG_BUFFER_SIZE];  //message buffer
const int UPDATE_RATE = 20;           //how often should we check for updates
const bool PEAK_MODE = true;        //Switch between peak or rms mode
const char days[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
const char SM_STR[13] = "SMALL.WAV";
const char SS_STR[13] = "SEASHELL.WAV";
const char LO_STR[13] = "LONG.WAV";
const char TEST_STR[13] = "TESTLOOP.WAV";
char FILE_NAME[13];     //empty string to be defined by a function, will take one of the 3 file names
int PLAYER_ID;          //the id of this player 0 1 or 2

#define SGTL_ERR_CODE 4
#define PLAYBACK_CODE
#define AWAKE_CODE
#define SD_ERR_CODE

//###########################################################################
// SETUP AND LOOP
//
void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);
  
  //WDT_timings_t config;
  //config.timeout = 5;
  //wdt.begin(config);
  
  // Log any crash report data
  if (CrashReport) {
    Serial.print("Previous crash detected: ");
    Serial.println(CrashReport);
    delay(1000);
  } else{
    Serial.println("No crash report available.");
  }

  setupPlayerID();

  // Initialize LED array pins
  for (int j = 0; j < 4; j++) {
    pinMode(LED_ARRAY[j], OUTPUT);
    digitalWrite(LED_ARRAY[j], LOW); // Start with LEDs off
  }
  Serial.println("LEDs array setup");

  // Initialize relay pins with safe state
  pinMode(REL_1, OUTPUT);
  digitalWrite(REL_1, LOW);
  pinMode(REL_2, OUTPUT);
  digitalWrite(REL_2, LOW);
  Serial.println("Relays array setup");

  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, LOW); // Start with PWM off
  Serial.println("PWM pin setup");

  // Audio memory allocation with error handling
  if (AudioMemoryUsage() > 0) {
    Serial.println("Audio memory already in use");
  } else{
    Serial.println("Audio memory is empty, let's allocate it.");
  }
  AudioMemory(64);
  
  // Enable audio codec with error checking
  while (!sgtl5000.enable()) {
    Serial.println("ERROR: Audio codec failed to enable");
    displayBinaryCode(13); // Error indication
    delay(1000);
  }
  sgtl5000.volume(audioVolume);
  Serial.println("Audio memory allocated");

  // Configure SPI for SD card with error handling
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  
  // SD card initialization with retry
  Serial.println("SD card initialization...");
  
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
      Serial.println("Unable to access the SD card");
      displayBinaryCode(3);
      delay(100);
    }
  } else {
    Serial.println("SD card loaded");
  }

  // RTC setup for LONG player only
  if (PLAYER_ID == 0) {
    setupRTC();
    Serial.println("RTC setup complete");
    
    Serial.print("Setup complete! Starting playback in ");
    Serial.print(STARTUP_DELAY / 1000);
    Serial.println("s.");
  } else {
    Serial.println("Setup complete! Waiting for LONG player commands.");
  }
  
  // Reset global state
  systemAwake = false;
  playbackStatus = false;
  trackIteration = 0;
  
  //wdt.reset();
}

//start millis thread timer
elapsedMillis pwmTimer;       // For PWM updates
elapsedMillis commandTimer;   // For checking commands
elapsedMillis statusTimer;    // For status updates
//elapsedMillis reportTimer;    // For periodic reporting (optional)

//LOOP
void loop() {
  //wdt.feed();

  if (statusTimer >= 1000) {
    statusUpdates();
    statusTimer = 0;
  }

  if (Serial.available()) {
    // Check for ':' (=message)
    if (Serial.peek() == ':') {
      checkUsbMessages();
      commandTimer = 0;
    } else {
      // Not a message, process as command
      checkUsbCommands();
      commandTimer = 0;
    }
  }

  //LO player
  if (PLAYER_ID == 0) {
    leader();
  }

  // SM/SS player
  if (PLAYER_ID != 0) {
    follower();
  }

  delay(5); //debounce
}
//###########################################################################
//#######                          HELPERS                            #######
//###########################################################################
//helper function to setup the RTC module - always updates time when connected via USB
void setupRTC() {
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);//wait for rtc to be connected
  }

  // Always update the time when connected via USB
  if (Serial) {
    // Time will be set based on when the code was compiled
    Serial.println("USB connected - updating RTC time from compilation timestamp");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    // Display the new time
    clockMe();
  } else if (rtc.lostPower()) {
    // Still update if power was lost, even if USB is not connected
    Serial.println("RTC lost power, setting time from compile timestamp");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

/*
 * helper function to write pwm output from peak or rms 
 * @pin: Pin to write PWM output.
 * @peak: TRUE for peak mode, FALSE for RMS mode.
 */
void writeOutPWM(uint8_t pin) {
  if (pwmTimer >= (unsigned long)pwmFreq) {
    pwmTimer = 0;  // Reset timer
    
    // Simplified peak/RMS handling
    if (PEAK_MODE) {
      if (audioPeak.available()) {
        int pwmValue = audioPeak.read() * rangePWM;
        analogWrite(pin, pwmValue);
      }
    } else {
      if (audioRMS.available()) {
        int pwmValue = audioRMS.read() * rangePWM;
        analogWrite(pin, pwmValue);
      }
    }
  }
}

void leader() {
  static elapsedMillis playbackTimer;
  static elapsedMillis updateTimer;
  static elapsedMillis serialCheckTimer;
  const unsigned long RETRY_INTERVAL = STARTUP_DELAY;
  
  // Check for playback (every 5 seconds)
  if (systemAwake && !wavPlayer.isPlaying() && playbackTimer >= RETRY_INTERVAL) {
    playbackTimer = 0;
    sendSerialCommand(CMD_PLAY);
    playAudio();
  }

  // Check for responses from followers
  if (serialCheckTimer >= 10) {  // Check every 10ms
    serialCheckTimer = 0;
    
    if (Serial3.available()) {
      // Check if it's a message starting with ':'
      if (Serial3.peek() == ':') {
        receiveSerialMessage();  // Process the incoming message from follower
      } else {
        // Clear any other characters
        while (Serial3.available()) {
          Serial3.read();
        }
      }
    }
  }
  
  // Update display and PWM output
  if (updateTimer >= UPDATE_RATE) {
    updateTimer = 0;
    if (systemAwake) {
      if (wavPlayer.isPlaying()) {
        writeOutPWM(PWM_PIN);
        if (knobCtrl){
          volumeControl();
        }
        displayBinaryCode(8);
      } else {
        displayBinaryCode(2);
        analogWrite(PWM_PIN, 0);  // Set PWM to zero
      }
    } else {
      // System is asleep
      if (wavPlayer.isPlaying()) {
        wavPlayer.stop();
      }
      displayBinaryCode(1);
      analogWrite(PWM_PIN, 0);  // Set PWM to zero
    }
  }
}

void follower() {
  static elapsedMillis commandCheckTimer;
  static elapsedMillis updateTimer;
  
  // Check for commands frequently
  if (commandCheckTimer >= 10) {
    commandCheckTimer = 0;
    
    if (Serial3.available()) {
      // Check if this is a message (starts with ':')
      if (Serial3.peek() == ':') {
        receiveSerialMessage();
      } else {
        // Process as a single command
        char inChar = (char)Serial3.read();
        processCommand(inChar);
      }
      
      // Clear buffer
      while (Serial3.available()) {
        Serial3.read();
      }
    }
  }

  // Update display and PWM output
  if (updateTimer >= UPDATE_RATE) {
    updateTimer = 0;
    
    if (systemAwake) {
      if (wavPlayer.isPlaying()) {
        writeOutPWM(PWM_PIN);
        displayBinaryCode(8);
      } else {
        displayBinaryCode(2);
        analogWrite(PWM_PIN, 0);  // Set PWM to zero
      }
    } else {
      if (wavPlayer.isPlaying()) {
        wavPlayer.stop();
      }
      displayBinaryCode(1);
      analogWrite(PWM_PIN, 0);  // Set PWM to zero
    }
  }
}

