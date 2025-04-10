/*
  Audio playback on a timer, and output audio RMS or peak values to PWM output

  The circuit on a teensy 4.0:
	- pin 6 as a digital output, using PWM (analogWrite function)
	- pins 18 and 19 used as SCL and SPA for the RTC module
  - auto ID player using pins 28 and 30
	- pwm output goes through an IRL520 MOSFET, that itself drives a LED strip.
  - relays are on 21, 20, 17 and 16

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

//OBJECTS
//audio
AudioPlaySdWav sdWav;
AudioAnalyzePeak audioPeak;
AudioAnalyzeRMS audioRMS;
AudioOutputI2S audioOutput;
AudioControlSGTL5000 sgtl5000;
//rtc
RTC_DS3231 rtc;

//AUDIO MATRIX
AudioConnection patchCord1(sdWav, 0, audioOutput, 0);
AudioConnection patchCord2(sdWav, 0, audioPeak, 0);
AudioConnection patchCord3(sdWav, 0, audioRMS, 0);

//SD CARD
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

//PINS
const int REL_1 = 16;
const int REL_2 = 17;
const int REL_3 = 20;
const int REL_4 = 21; //relay 4 is never used
const int LED_1 = 2;
const int LED_2 = 3;
const int LED_3 = 4;
const int LED_4 = 5;
const int PWM_PIN = 6;
const int STATUS_PIN = 22;
const int PLAYBACK_PIN = 23;
const int SMALL_PIN = 30;
const int SEASHELL_PIN = 28;

const uint8_t VOL_CTRL_PIN = A0;
const uint8_t PWM_CTRL_PIN = A1;

//VARIABLES
float audioVolume = 0.2;
int rangePWM = 255;
bool systemStatus = false;
bool playbackStatus = false;
const bool PEAK_MODE = false;
const char days[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int START_HOUR = 6;
const int END_HOUR = 23;
String SM_STR = "SMALL.WAV";
String SS_STR = "SEASHELL.WAV";
String LO_STR = "LONG.WAV";
String FILE_NAME = "temp string";
int PLAYER_ID = 4;

const uint8_t REL_ARRAY[4] = {REL_1, REL_2, REL_3, REL_4};
const uint8_t LED_ARRAY[4] = {LED_1, LED_2, LED_3, LED_4};

//SETUP
void setup() {
  Serial.begin(57600);

  setupPlayerID();

  for (int j=0;j<4;j++){
    pinMode(LED_ARRAY[j],OUTPUT);
  }
  Serial.println("Leds array setup");

  for (int i=0;i<4;i++){
    pinMode(REL_ARRAY[i],OUTPUT);
  }
  Serial.println("Relays array setup");

  pinMode(PWM_PIN, OUTPUT);
  pinMode(STATUS_PIN, OUTPUT);
  pinMode(PLAYBACK_PIN, OUTPUT);
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
  if (PLAYER_ID == 0){
    setupRTC();
    Serial.println("RTC setup");

    //flag for initialization complete
    Serial.println("Setup complete!");

    delay(5000); //wait for 2 other units to listen
  } else {
    //flag for initialization complete
    Serial.println("Setup complete! waiting for LONG player");
  }  
}

//start millis thread timer
elapsedMillis fps;

//LOOP
void loop() {
  statusUpdates();

  if (PLAYER_ID == 0){
    digitalWrite(LED_1, HIGH);
  } else if (PLAYER_ID == 1){
  digitalWrite(LED_2, HIGH);
  }  else if (PLAYER_ID == 2){
  digitalWrite(LED_3, HIGH);
  }
}

//###########################################################################
//helper function to setup the RTC module (adafruit documentation, details in "Examples > RTClib")
void setupRTC(){
  /*
  #ifndef ESP8266
    while (!Serial); // wait for serial port to connect. Needed for native USB
  #endif
  */

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
}

//helper function to print time
void clockMe(){
  DateTime input = rtc.now();
  Serial.print(input.year(), DEC);
  Serial.print('/');
  Serial.print(input.month(), DEC);
  Serial.print('/');
  Serial.print(input.day(), DEC);
  Serial.print(" (");
  Serial.print(days[input.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(input.hour(), DEC);
  Serial.print(':');
  Serial.print(input.minute(), DEC);
  Serial.print(':');
  Serial.print(input.second(), DEC);
  Serial.println();
  delay(1000);
}

/*
 * helper function to write pwm output from peak or rms 
 * @pin: Pin to write PWM output.
 * @peak: TRUE for peak mode, FALSE for RMS mode.
 */
void writeOutPWM(uint8_t pin, bool peak){
  int pwm;

  if (fps > 24) {
    if (peak == true){ //peak mode
      if (audioPeak.available()) {
        fps = 0;
        pwm = audioPeak.read() * rangePWM;
        analogWrite(pin, pwm);
      }

    } else if (peak == false) { //RMS mode±
      if (audioRMS.available()) {
        fps = 0;
        pwm = audioRMS.read() * rangePWM;
        analogWrite(pin, pwm);
      }
    }
  }
}

//helper function to control the audio volume
void volumeControl(){
  float val = analogRead(VOL_CTRL_PIN);
  audioVolume = val / 1024; //10bits to 0-1 scale
  sgtl5000.volume(audioVolume);
}

//helper function to control the PWM range
void pwmControl(){
  int val = analogRead(PWM_CTRL_PIN);
  rangePWM = val / 4; //10bits to 0-255 scale
}

/*
 * helper function to display a number on the LEDs
 * @mode: true to turn on all LEDs, false to turn them off.
 */
void ledsArrayOnOff(bool mode){
  if (mode == true){
    digitalWrite(LED_1, HIGH);
    digitalWrite(LED_2, HIGH);
    digitalWrite(LED_3, HIGH);
    digitalWrite(LED_4, HIGH);
  } else{
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
  }
}

//helper function to sequence startup of the psu, amp then speaker
void startupSequence(){
  digitalWrite(STATUS_PIN, HIGH);//send status signal to the other units
  digitalWrite(REL_1, HIGH); //opens 230V AC Line to the PSU
  delay(500);
  digitalWrite(REL_2, HIGH); //opens 36V DC line from the PSU to the amplifier
  delay(500);
  digitalWrite(REL_3, HIGH); //opens audio line from the amplifier to the speaker
  systemStatus = true;
}

//helper function to sequence shutdown of the speaker, amp then psu
void shutDownSequence(){
  digitalWrite(STATUS_PIN, HIGH);//send status signal to the other units
  digitalWrite(REL_3, LOW); //closes audio line from the amplifier to the speaker
  delay(500);
  digitalWrite(REL_2, LOW); //closes 36V DC line from the PSU to the amplifier
  delay(500);
  digitalWrite(REL_1, LOW); //closes 230V AC Line to the PSU
  systemStatus = false;
}

//helper function to manage audio playback
void playAudio(){
  //sdWav.play(FILE_NAME);
  playbackStatus = true;

  Serial.print("Start playing ");
  Serial.println(FILE_NAME);
}

//helper function to manage audio status and playback status booleans
void statusUpdates(){
  //system status
  if (PLAYER_ID == 0){  //if LONG player
    if (rtc.now().hour() >= START_HOUR && rtc.now().hour() < END_HOUR) {
      systemStatus = true;
    } else {
      systemStatus = false;
    }
  } else {              //if SMALL/SEASHELL player
    if (digitalRead(STATUS_PIN) == HIGH){
      systemStatus = true;
    } else {
      systemStatus = false;      
    }
  }

  //playback status
  if (sdWav.isPlaying() == false) {
    playbackStatus = false;
  } else{
    playbackStatus = true;
  }
}

//helper function to setup playerID, pins 28 or 30 are connected to 3.3V. 0 = LONG, 1 = SMALL, 2 = SEASHELL
void setupPlayerID(){
  pinMode(SMALL_PIN,INPUT);
  pinMode(SEASHELL_PIN,INPUT);

  if (digitalRead(SMALL_PIN) == HIGH){
    PLAYER_ID = 1;
    FILE_NAME = SM_STR;
  } else if (digitalRead(SEASHELL_PIN) == HIGH){
    PLAYER_ID = 2;
    FILE_NAME = SS_STR;
  } else {
  PLAYER_ID = 0;
  FILE_NAME = LO_STR;
  }

  Serial.print("Audio file setup ");
  Serial.println(FILE_NAME);
}

void leader(){
  //IF SYSTEM IS AWAKE
  if (systemStatus == true) {
    startupSequence();

    if (playbackStatus == false){
      playAudio();
      delay(10);
    }

    //write PWM during playback
    while (sdWav.isPlaying() == true) {
      writeOutPWM(PWM_PIN, PEAK_MODE);
      //volumeControl();
      //pwmControl();
    }

    while (sdWav.isPlaying() == false) {
      digitalWrite(PWM_PIN, LOW);
      digitalWrite(PLAYBACK_PIN, LOW);
      playbackStatus = false;
      Serial.println("Playback finished");
      delay(1000);
    }
  } 

  //IF SYSTEM IS ASLEEP
  else{
    shutDownSequence();
    digitalWrite(PWM_PIN, LOW);
    clockMe();
    Serial.print("I'm asleep and will wake up at ");
    Serial.println(START_HOUR);
  }
}

void follower(){
  //implement me
}
