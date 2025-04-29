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
    16 REL_1 (relay PSU/amp)
    17 REL_2 (relay speaker)
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
const int REL_1 = 16;
const int REL_2 = 17;
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
int audioMemory = 8;
int startupDelay = 5000;   //inactivity time after setup for LONG player
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
char FILE_NAME[13] = "";
const char* TEST_STR = "LONG_TEST_LOOP.WAV";
int PLAYER_ID;

//commands (on USB serial and Serial3) 
const char CMD_LED_1 = '1';
const char CMD_LED_2 = '2';
const char CMD_LED_3 = '3';
const char CMD_LED_4 = '4';
const char* CMD_HELP = "help";
const char* CMD_WAKEUP = "wakeup";
const char* CMD_PLAY = "play";
const char* CMD_SLEEP = "sleep";
const char* CMD_STOP = "stop";
const char* CMD_REPLAY = "replay";
const char* CMD_REPORT = "report";
const char* CMD_VOL = "vol";                // followed by a float value in the 0-1 range e.g. vol0.8
const char* CMD_LED = "led";                // followed by an int value in the 0-15 range e.g. led15
const char* CMD_PWM_RANGE = "pwm";          // followed by a float value in the 0-1 range e.g. pwm1.0
const char* CMD_PWM_FREQ = "freq";          // followed by an int value in the 0-100 range e.g. freq25

//SETUP
void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);

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
  AudioMemory(8);
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
    Serial.print(startupDelay / 1000);
    Serial.println("s.");

    delay(startupDelay);  //wait for 2 other units to listen
  } else {
    //flag for initialization complete
    Serial.println("Setup complete! waiting for LONG player to playback.");
  }
}

//start millis thread timer
elapsedMillis fps;

//LOOP
void loop() {
  //testLoop();
  statusUpdates();
  checkUsbCommands();

  //LO player
  if (PLAYER_ID == 0) {
    leader();
  }

  // SM/SS player
  if (PLAYER_ID != 0) {
    follower();
  }
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
  int pwm;

  if (fps > pwmFreq) {
    if (PEAK_MODE) {  //peak mode
      if (audioPeak.available()) {
        fps = 0;
        pwm = audioPeak.read() * rangePWM;
        analogWrite(pin, pwm);
      }

    } else if (!PEAK_MODE) {  //RMS mode
      if (audioRMS.available()) {
        fps = 0;
        pwm = audioRMS.read() * rangePWM;
        analogWrite(pin, pwm);
      }
    }
  }
}

void leader() {
  if (systemAwake) {
    

    if (!wavPlayer.isPlaying()) {

      //delay(1000); //wait a second for followers relays to activate
      sendSerialCommand(CMD_PLAY);
      playAudio();
      clockMe();
    }

    if (wavPlayer.isPlaying()) {
      writeOutPWM(PWM_PIN);
      displayBinaryCode(8);
      sendSerialCommand("led8");
    }
  }

  if (!systemAwake) {
    if (wavPlayer.isPlaying()) {
      wavPlayer.stop();
      sendSerialCommand(CMD_STOP);
      digitalWrite(PWM_PIN, LOW);
    }
    if (!wavPlayer.isPlaying()) {
      sendSerialCommand("led1");
      displayBinaryCode(1);
      digitalWrite(PWM_PIN, LOW);
    }
  }
}

void follower() { 
  checkSerialCommands();

  if (systemAwake){
    if (wavPlayer.isPlaying()){
      writeOutPWM(PWM_PIN);
    }
  }

  if (!systemAwake) {
    if (wavPlayer.isPlaying()) {
      digitalWrite(PWM_PIN, LOW);
    }
    if (!wavPlayer.isPlaying()) {
      digitalWrite(PWM_PIN, LOW);
    }
  }
}

//if test mode
void testLoop() {
  if (!wavPlayer.isPlaying()) {
    startupSequence();
    testAudio();
  }

  while (wavPlayer.isPlaying()) {
    writeOutPWM(PWM_PIN);
  }
}

//helper function to manage audio playback
void testAudio() {
  wavPlayer.play(TEST_STR);
  delay(10);
  trackIteration += 1;
  Serial.print("Start playing test ");
  Serial.println(TEST_STR);
  Serial.print("Test iteration nr ");
  Serial.print(trackIteration);
  Serial.println(" during curent session (will be deleted tomorrow morning at 6AM).");
}