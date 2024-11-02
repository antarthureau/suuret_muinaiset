/*
  Audio playback on a timer, and output audio RMS or peak values to PWM output

  The circuit on a teensy 3.2:
	- pin 6 as a digital output, using PWM (analogWrite function)
	- pins 18 and 19 used as SCL and SPA for the RTC module (Adafruit 8523)
	- pwm output goes through an IRL520 MOSFET, that itself drives a LED strip.

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
#include "RTClib.h"

//OBJECTS
//audio
AudioPlaySdWav playSdWav1;
AudioAnalyzePeak audioPeak;
AudioAnalyzeRMS audioRMS;
AudioOutputI2S audioOutput;
AudioControlSGTL5000 sgtl5000_1;
//rtc
RTC_PCF8523 rtc;

//AUDIO MATRIX
AudioConnection patchCord1(playSdWav1, 0, audioOutput, 0);
AudioConnection patchCord2(playSdWav1, 0, audioPeak, 0);
AudioConnection patchCord3(playSdWav1, 0, audioRMS, 0);

//SD CARD
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

//PINS
const int PWM_PIN = 6;
const int RELAY_PIN = 16;
const uint8_t VOL_CTRL_PIN = A0;
const uint8_t PWM_CTRL_PIN = A1;

//VARIABLES
float audioVolume = 0.5;
int rangePWM = 255;
const bool PEAK_MODE = false;
char days[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int START_HOUR = 6;
const int END_HOUR = 23;
const char FILE_NAME[] = "SPIKY.wav"; //LONG.wav, SMALL.wav or SPIKY.wava depending on player

//SETUP
void setup() {
  Serial.begin(57600);

  //audio memory allocation, codec and volume setup
  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(audioVolume);

  // Configure SPI communication for the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  
  // Check SD card accessibility
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  } else {
    Serial.println("SD card loaded");
  }

  //Declare pins
  pinMode(PWM_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  //help function to init RTC
  setupRTC();

  //flash light 4 times at startup (1 sec)
  for (int i=0; i < 4; i++){
    analogWrite(PWM_PIN, 1);
    delay(50);
    analogWrite(PWM_PIN, 0);
    delay(50);
  }
  
  //flag for initialization complete
  Serial.println("Initialized");
}


//start millis thread timer
elapsedMillis fps;

//LOOP
void loop() {
  if (rtc.now().hour() >= START_HOUR){
      if (rtc.now().hour() <= END_HOUR){

        digitalWrite(RELAY_PIN, HIGH);
        if (playSdWav1.isPlaying() == false) {
          Serial.print("Start playing ");
          Serial.println(FILE_NAME);
          playSdWav1.play(FILE_NAME);
        }
        //while playing
        while (playSdWav1.isPlaying() == true) {
          writeOutPWM(PWM_PIN, PEAK_MODE);
          volumeControl();
          //pwmControl();
        }
        clockMe();
        }
    }

  else{
    digitalWrite(RELAY_PIN, LOW);
    analogWrite(PWM_PIN, 0);
    Serial.print("I'm asleep. ");
    Serial.print(FILE_NAME);
    Serial.print(" will play again from ");
    Serial.println(START_HOUR);
    clockMe();
  }

}

//###########################################################################
//helper function to setup the RTC module (adafruit documentation, details in "Examples > RTClib > pcf8523")
void setupRTC(){
  //comment-out the 3 next lines when not on USB
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

  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");

    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  }

  //uncomment next line to update RTC time
  //If used, be sure to compile 2 times! first with the line uncommented setup the RTC, a second time with the line commented
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  rtc.start();

  float drift = 43;
  float period_sec = (7 * 86400);
  float deviation_ppm = (drift / period_sec * 1000000);
  float drift_unit = 4.34;
  int offset = round(deviation_ppm / drift_unit);

  clockMe(); //print date and time
  Serial.print("Offset is "); // Print offset time
  Serial.println(offset); // Print offset time
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
void writeOutPWM(int pin, bool peak){
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
  sgtl5000_1.volume(audioVolume);
}

//helper function to control the PWM range
void pwmControl(){
  int val = analogRead(PWM_CTRL_PIN);
  rangePWM = val / 4; //10bits to 0-255 scale
}
