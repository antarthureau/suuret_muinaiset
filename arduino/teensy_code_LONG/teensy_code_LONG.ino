/*
  Sound to light system. Audio playback outputs audio RMS or peak values to PWM output, driving LED strip.
  3 players: 1 leader (LO short for LONG) has an RTC module and control 2 followers through Serial3 (SM for SMALL and SS for SEASHELL)

  PINS:
    0 x
    1 x
    2 LED_1
    3 LED_2
    4 LED_3
    5 LED_4
    6 PWM_OUT (LED strip control)
    7 x
    8 x
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
AudioPlaySdWav           sdWav;
AudioAnalyzePeak         audioPeak;
AudioAnalyzeRMS          audioRMS;
AudioOutputI2S           audioOutput;
AudioControlSGTL5000     sgtl5000;
//rtc
RTC_DS3231 rtc;

//AUDIO MATRIX
AudioConnection patchCord1(sdWav, 0, audioOutput, 0);
AudioConnection patchCord2(sdWav, 0, audioPeak, 0);
AudioConnection patchCord3(sdWav, 0, audioRMS, 0);

//SD CARD
const int SDCARD_CS_PIN = 10;
const int SDCARD_MOSI_PIN = 11; //teensy uses 11 instead of 7
const int SDCARD_SCK_PIN = 13; //teensy uses 13 instead of 14

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

//SYSTEM
float audioVolume = 0.8;
int rangePWM = 255;
bool systemAwake = false;
bool playbackStatus = false;
const bool PEAK_MODE = false; //Switch between peak or rms mode

//RTC
const char days[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int START_HOUR = 6;
const int END_HOUR = 23;

//FILE NAMES AND PLAYER ID
const char SM_STR[13] = "SMALL.WAV";
const char SS_STR[13] = "SEASHELL.WAV";
const char LO_STR[13] = "LONG.WAV";
char FILE_NAME[13] = "";
int PLAYER_ID;

//IO ARRAYS
const uint8_t LED_ARRAY[4] = {LED_1, LED_2, LED_3, LED_4};

//SETUP
void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);

  setupPlayerID();

  for (int j=0;j<4;j++){
    pinMode(LED_ARRAY[j],OUTPUT);
  }
  Serial.println("Leds array setup");

  pinMode(REL_1,OUTPUT);
  pinMode(REL_2,OUTPUT);
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

/*
* helper function to write on the all four LEDs on the LEDs array
* @valLed1: value for LED_1
* @valLed2: value for LED_2
* @valLed3: value for LED_3
* @valLed4: value for LED_4
*/
void setLedPattern(bool valLed1, bool valLed2, bool valLed3, bool valLed4){
  bool valuesArray[4] = {valLed1, valLed2, valLed3, valLed4};

  for (int i=0; i<4; i++){
    digitalWrite(LED_ARRAY[i], valuesArray[i]);
  }
}

/*
 * helper function to display a status code from 0-15 on the 4 LEDs array
 * @code: integer from 0-15 (included)
 */
void displayBinaryCode(int code){
  if (code >= 0 && code <= 15){
    bool bits[4];
    
    // Convert the integer to its binary representation (4 bits)
    for (int i = 3; i >= 0; i--) {
      bits[i] = (code & 1);  // Check if the least significant bit is 1
      code >>= 1;            // Right shift to check the next bit
    }
    
    // Call ledsArrayDigit with the 4 bits
    setLedPattern(bits[0], bits[1], bits[2], bits[3]);
  } else {
    Serial.println("Status code should be an integer in the 0-15 range");
  }
}

//helper function to sequence startup of the psu, amp then speaker
void startupSequence(){
  digitalWrite(REL_1, HIGH); //turns amp on
  delay(250);
  digitalWrite(REL_2, HIGH); //turns speaker on
}

//helper function to sequence shutdown of the speaker, amp then psu
void shutDownSequence(){
  digitalWrite(REL_2, LOW); //turns speaker off
  delay(250);
  digitalWrite(REL_1, LOW); //turns amp off
}

//helper function to manage audio playback
void playAudio(){
  sdWav.play(FILE_NAME);
  delay(10);

  Serial.print("Start playing ");
  Serial.println(FILE_NAME);
}

//helper function to manage audio status and playback status booleans
void statusUpdates(){
  if (PLAYER_ID == 0){ 
    if (rtc.now().hour() >= START_HOUR && rtc.now().hour() < END_HOUR) {
      if (!systemAwake){
        startupSequence();
        systemAwake = !systemAwake;
      }
    } else {
      if (systemAwake){
        shutDownSequence();
        systemAwake = !systemAwake;
      }
    }
  }
}

//helper function to setup playerID, pins 28 or 30 are connected to 3.3V. 0 = LONG, 1 = SMALL, 2 = SEASHELL
void setupPlayerID(){
  pinMode(SMALL_PIN,INPUT);
  pinMode(SEASHELL_PIN,INPUT);
  pinMode(LONG_PIN, INPUT);

  if (digitalRead(SMALL_PIN) == HIGH){
    PLAYER_ID = 1;
    strcpy(FILE_NAME, SM_STR);
  }
  
  else if (digitalRead(SEASHELL_PIN) == HIGH){
    PLAYER_ID = 2;
    strcpy(FILE_NAME, SS_STR);
  }
  
  else if (digitalRead(LONG_PIN) == HIGH){
    PLAYER_ID = 0;
    strcpy(FILE_NAME, LO_STR);
  }

  Serial.print("Audio file setup ");
  Serial.println(FILE_NAME);
}

void leader(){
  if (systemAwake){
    Serial3.write('W');
    displayBinaryCode(15);

    if (!sdWav.isPlaying()){
      Serial3.write('P');
      playAudio();
    }

    if(sdWav.isPlaying()){
      writeOutPWM(PWM_PIN, true);
      //volumeControl();
    }
  }

  if (!systemAwake){
    Serial3.write('S');
    displayBinaryCode(1);
  }
}

void follower(){
  if (Serial3.available() > 0) {
    char incomingByte = Serial3.read();

    switch (incomingByte) {
      case 'A':
        displayBinaryCode(1);
        break;

      case 'B':
        displayBinaryCode(2);
        break;

      case 'C':
        displayBinaryCode(4);
        break;

      case 'D':
        displayBinaryCode(8);
        break;

      case 'P':
        playAudio();
        break;

      case 'W':
        if (!systemAwake){
          systemAwake = !systemAwake;
          startupSequence();
        }  
        break;

      case 'S':
        if (systemAwake){
          systemAwake = !systemAwake;
          shutDownSequence();
        }  
        break;
      
      case 'M':
        audioVolume = 0;
        break;

      default:
        displayBinaryCode(0);
        break;
    }
  } 

  if (systemAwake){
    displayBinaryCode(15);
    if(sdWav.isPlaying()){
      writeOutPWM(PWM_PIN, true);
    }
  }
  if (!systemAwake){
    displayBinaryCode(1);
  }
}