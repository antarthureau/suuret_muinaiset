/*
  Sound to light system. Audio playback outputs audio RMS or peak values to PWM output, driving LED strip.
  3 players: 1 leader (LO short for LONG) has an RTC module and control 2 followers through Serial3 (SM for SMALL and SS for SEASHELL)

  PINS:
    0 x (must be NC, used for USB Serial)
    1 x (must be NC, used for USB Serial)
    2 LED_1
    3 LED_2
    4 LED_3
    5 LED_4
    6 PWM_OUT (LED strip control)
    7 x (can be used as receiver for Serial2)
    8 x (can be used as transmitter for Serial2)
    9 x
    10 SDCARD_CS_PIN (SD card)
    11 SDCARD_MOSI_PIN (SD card)
    12 x
    13 SDCARD_SCK_PIN (SD card)
    14 RX3 (receive serial on SM/SS, x on LO)
    15 TX3 (send serial on LO, x on SM/SS)
    16 REL_2 (relay speaker)
    17 REL_1 (relay PSU/amp)
    18 SDA0 (RTC on LO, x on SM/SS)
    19 SCL0 (RTC on LO, x on SM/SS)
    20 LRCLK1 (audio)
    21 BCLK1 (audio)
    22 VOL_CTRL_PIN (analog volume ctrl)
    23 MCLK (audio)
    28 PLAYED_ID (2, SS)
    30 PLAYER_ID (1, SM)
    32 PLAYER_ID (0, LO)

  CODES:
    0 x
    1 asleep
    2 awake and not playing
    3
    4
    5
    6
    7
    8 playing audio
    9
    10
    11
    12
    13
    14
    15 awake

  created 21.08.2024
  by Antoine "Arthur" Hureau-Parreira
  Ant Art Enk, Bergen NO

  This code relies on open-source technology and references and belongs to the public domain.
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

//OBJECTS
//audio
AudioPlaySdWav wavPlayer;
AudioAnalyzePeak audioPeak;
AudioAnalyzeRMS audioRMS;
AudioOutputI2S audioOutput;
AudioControlSGTL5000 sgtl5000;
//RTC
RTC_DS3231 rtc;

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
const uint8_t VOL_CTRL_PIN = A8;

//IO ARRAYS
const uint8_t LED_ARRAY[4] = {LED_1, LED_2, LED_3, LED_4};

//SYSTEM
float audioVolume = 0.8;  //0-1, controls loudness
int rangePWM = 255;       //0-255, controls brightness
int currentCode = 0;      //starts at 0
const int STARTUP_DELAY = 5000;   //inactivity time after setup for LONG player
int trackIteration = 0;    //resets every day at START_HOUR
const int START_HOUR = 6;  //daily wake-up time
const int END_HOUR = 23;   //daily sleep time
int pwmFreq = 25;   //Hz, refresh rate for the PWM
const int REL_SW_DELAY = 500;
bool systemAwake = false;  //activity time between START_HOUR and END_HOUR
bool playbackStatus = false;
const bool PEAK_MODE = true;  //Switch between peak or rms mode
const char days[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
const char SM_STR[13] = "SMALL.WAV";
const char SS_STR[13] = "SEASHELL.WAV";
const char LO_STR[13] = "LONG.WAV";
char FILE_NAME[13];
int PLAYER_ID;

//commands (on USB serial and Serial3) 
#define CMD_LED_1 '1'  // LED 1 control
#define CMD_LED_2 '2'  // LED 2 control
#define CMD_LED_3 '3'  // LED 3 control
#define CMD_LED_4 '4'  // LED 4 control
#define CMD_HELP 'h'   // Display help
#define CMD_WAKEUP 'w' // Wake up system
#define CMD_PLAY 'p'   // Play audio
#define CMD_SLEEP 's'  // Sleep system
#define CMD_STOP '!'   // Stop audio
#define CMD_REPLAY 'z' // Reset and replay audio
#define CMD_REPORT 'r' // Generate system report
#define CMD_VOL_UP '+'  // Increase volume
#define CMD_VOL_DOWN '-' // Decrease volume
#define CMD_PWM_UP '>'  // Increase PWM range
#define CMD_PWM_DOWN '<' // Decrease PWM range

//SETUP
void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);

  if (CrashReport){
    Serial.print(CrashReport);
    delay(STARTUP_DELAY);
  }

  setupPlayerID();

  for (int j = 0; j < 4; j++) {
    pinMode(LED_ARRAY[j], OUTPUT);
  }
  Serial.println("Leds array setup");

  pinMode(REL_1, OUTPUT);
  pinMode(REL_2, OUTPUT);
  Serial.println("Relays array setup");

  pinMode(PWM_PIN, OUTPUT);
  Serial.println("PWM and listen pins setup");

  //audio memory allocation, codec and volume setup
  AudioMemory(128);
  sgtl5000.enable();
  sgtl5000.volume(audioVolume);

  Serial.println("Audio memory allocated");

  // Configure SPI communication for the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  Serial.println("SPI communication configures");
  Serial.println("SD card initialization...");

  // Check SD card accessibility
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  } else {
    Serial.println("SD card loaded");
  }

  //help function to init RTC
  if (PLAYER_ID == 0) {
    setupRTC();
    Serial.println("RTC setup");

    //flag for initialization complete
    Serial.print("Setup complete! starting playback in ");
    Serial.print(STARTUP_DELAY / 1000);
    Serial.println("s.");

    //delay(STARTUP_DELAY);  //wait for 2 other units to listen
  } else {
    //flag for initialization complete
    Serial.println("Setup complete! waiting for LONG player to playback.");
  }
  
}

//start millis thread timer
elapsedMillis pwmTimer;       // For PWM updates
elapsedMillis commandTimer;   // For checking commands
elapsedMillis statusTimer;    // For status updates
elapsedMillis displayTimer;   // For display updates
elapsedMillis reportTimer;    // For periodic reporting (optional)

//LOOP
void loop() {
  if (statusTimer >= 1000) {
    statusUpdates();
    statusTimer = 0;
  }
  
  if (commandTimer >= 20) {
    checkUsbCommands();
    commandTimer = 0;
  }

  //LO player
  if (PLAYER_ID == 0) {
    leader();
  }

  // SM/SS player
  if (PLAYER_ID != 0) {
    follower();
  }

  delay(5);
}


//###########################################################################
//helper function to setup the RTC module (adafruit documentation, details in "Examples > RTClib")
void setupRTC() {
  /*
  #ifndef ESP8266
    while (!Serial); // wait for serial port to connect. Needed for native USB
  #endif
  */

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

/*
 * helper function to write pwm output from peak or rms 
 * @pin: Pin to write PWM output.
 * @peak: TRUE for peak mode, FALSE for RMS mode.
 */
void writeOutPWM(uint8_t pin) {
  if (pwmTimer >= pwmFreq) {
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
  const unsigned long RETRY_INTERVAL = 5000;
  
  if (systemAwake) {
    if (!wavPlayer.isPlaying() && playbackTimer >= RETRY_INTERVAL) {
      playbackTimer = 0;
      sendSerialCommand(CMD_PLAY);
      playAudio();
    }

    if (wavPlayer.isPlaying()) {
      writeOutPWM(PWM_PIN);
      displayBinaryCode(8);
    } else {
      displayBinaryCode(2);
      digitalWrite(PWM_PIN, LOW);
    }
  } else {
    // System is asleep
    if (wavPlayer.isPlaying()) {
      wavPlayer.stop();
    }
    displayBinaryCode(1);
    digitalWrite(PWM_PIN, LOW);
  }
}

void follower() {
  static elapsedMillis commandCheckTimer;
  static elapsedMillis displayUpdateTimer;
  
  // Check for commands more frequently
  if (commandCheckTimer >= 10) { // 100Hz command checking
    commandCheckTimer = 0;
    
    if (Serial3.available()) {
      char inChar = (char)Serial3.read();
      
      // Process commands immediately
      switch(inChar) {
        case CMD_WAKEUP:
          Serial.println("Wake command received");
          startupSequence();
          break;
        
        case CMD_SLEEP:
          Serial.println("Sleep command received");
          shutDownSequence();
          Serial.println("System going to sleep");
          break;
  
        case CMD_REPORT:
          Serial.println("Report command received");
          systemReport(PLAYER_ID);
          break;
          
        case CMD_PLAY:
          if (systemAwake) {
            Serial.println("Play command received");
            playAudio();
          }
          break;
          
        case CMD_STOP: // Stop
          Serial.println("Stop command received");
          wavPlayer.stop();
          break;

        case CMD_REPLAY: // Stop
          Serial.println("Replay command received");
          wavPlayer.stop();
          playAudio();
          break;
          
        case CMD_VOL_UP: // Volume up
          audioVolume += 0.1f;
          if (audioVolume > 1.0f) audioVolume = 1.0f;
          sgtl5000.volume(audioVolume);
          Serial.print("Volume increased to: ");
          Serial.println(audioVolume);
          break;
          
        case CMD_VOL_DOWN: // Volume down
          audioVolume -= 0.1f;
          if (audioVolume < 0.0f) audioVolume = 0.0f;
          sgtl5000.volume(audioVolume);
          Serial.print("Volume decreased to: ");
          Serial.println(audioVolume);
          break;
          
        case CMD_PWM_UP: // PWM range up
          rangePWM += 25; // Increase by ~10% of max (255)
          if (rangePWM > 255) rangePWM = 255;
          Serial.print("PWM range increased to: ");
          Serial.println(rangePWM);
          break;
          
        case CMD_PWM_DOWN: // PWM range down
          rangePWM -= 25; // Decrease by ~10% of max (255)
          if (rangePWM < 0) rangePWM = 0;
          Serial.print("PWM range decreased to: ");
          Serial.println(rangePWM);
          break;
      }
      
      // Clear buffer
      while (Serial3.available()) {
        Serial3.read();
      }
    }
  }
  
  if (displayUpdateTimer >= 20) {
    displayUpdateTimer = 0;
    
    if (systemAwake) {
      if (wavPlayer.isPlaying()) {
        writeOutPWM(PWM_PIN);
        displayBinaryCode(8);
      } else {
        displayBinaryCode(2);
        digitalWrite(PWM_PIN, LOW);
      }
    } else {
      if (wavPlayer.isPlaying()) {
        wavPlayer.stop();
      }
      displayBinaryCode(1);
      digitalWrite(PWM_PIN, LOW);
    }
  }
}