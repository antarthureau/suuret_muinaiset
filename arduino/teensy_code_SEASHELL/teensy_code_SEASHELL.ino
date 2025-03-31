/*
  Audio playback on a timer, and output audio RMS or peak values to PWM output

  The circuit on a teensy 4.0:
	- pin 6 as a digital output, using PWM (analogWrite function)
	- pwm output goes through an IRL520 MOSFET, that itself drives a LED strip.
  - relays are on 21, 20, 17 and 16

  created 21.08.2024
  by Antoine "Arthur" Hureau-Parreira
  Ant Art Enk, Bergen NO

  This code relies on open-source technology and references and belongs to the public domain.

  This is SMALL player
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <elapsedMillis.h>

//OBJECTS
//audio
AudioPlaySdWav playSdWav1;
AudioAnalyzePeak audioPeak;
AudioAnalyzeRMS audioRMS;
AudioOutputI2S audioOutput;
AudioControlSGTL5000 sgtl5000_1;

//AUDIO MATRIX
AudioConnection patchCord1(playSdWav1, 0, audioOutput, 0);
AudioConnection patchCord2(playSdWav1, 0, audioPeak, 0);
AudioConnection patchCord3(playSdWav1, 0, audioRMS, 0);

//SD CARD
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

//PINS
const int REL_1 = 21;
const int REL_2 = 20;
const int REL_3 = 17;
const int REL_4 = 16; //relay 4 is never used
const int LED_1 = 2;
const int LED_2 = 3;
const int LED_3 = 4;
const int LED_4 = 5;
const int PWM_PIN = 6;
const int TRIGGER_1 = 22;
const int TRIGGER_2 = 23;

const uint8_t VOL_CTRL_PIN = A0;
const uint8_t PWM_CTRL_PIN = A1;

//VARIABLES
float audioVolume = 0.2;
int rangePWM = 255;
const bool PEAK_MODE = false;
const char FILE_NAME[] = "SEASHELL.wav"; //LONG.wav, SMALL.wav or SEASHELL.wav depending on player

const uint8_t REL_ARRAY[4] = {REL_1, REL_2, REL_3, REL_4};
const uint8_t LED_ARRAY[4] = {LED_1, LED_2, LED_3, LED_4};

//SETUP
void setup() {
  Serial.begin(57600);

  for (int j=0;j<4;j++){
    pinMode(LED_ARRAY[j],OUTPUT);
  }
  Serial.println("Leds array setup");

  for (int i=0;i<4;i++){
    pinMode(REL_ARRAY[i],OUTPUT);
  }
  Serial.println("Relays array setup");

  pinMode(PWM_PIN, OUTPUT);
  pinMode(TRIGGER_1, INPUT);
  pinMode(TRIGGER_2, INPUT);
  Serial.println("PWM and listen pins setup");

  //audio memory allocation, codec and volume setup
  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(audioVolume);

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

  
  //flag for initialization complete
  Serial.println("Setup complete!");
}


//start millis thread timer
elapsedMillis fps;

//LOOP
void loop() {
  bool status = digitalRead(TRIGGER_1); //reads system status for the relays
  bool playback = digitalRead(TRIGGER_2); //reads playback status

  if (status == true){
    digitalWrite(REL_1, HIGH); //opens 230V AC Line to the PSU
    delay(500);
    digitalWrite(REL_2, HIGH); //opens 36V DC line from the PSU to the amplifier
    delay(500);
    digitalWrite(REL_3, HIGH); //opens audio line from the amplifier to the speaker
    Serial.println("startup trigger received!");
  } else{
    digitalWrite(REL_3, LOW); //closes audio line from the amplifier to the speaker
    delay(500);
    digitalWrite(REL_2, LOW); //closes 36V DC line from the PSU to the amplifier
    delay(500);
    digitalWrite(REL_1, LOW); //closes 230V AC Line to the PSU
    digitalWrite(PWM_PIN, LOW);

    Serial.println("I'm asleep ");
  }

  if (playback == true){
    if (playSdWav1.isPlaying() == false) {
      Serial.print("playback trigger received! Start playing ");
      Serial.println(FILE_NAME);
      playSdWav1.play(FILE_NAME);
    }

    //while playing
    while (playSdWav1.isPlaying() == true) {
      writeOutPWM(PWM_PIN, PEAK_MODE);
      volumeControl();
      //pwmControl();
    }
  }
}

//###########################################################################
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
  sgtl5000_1.volume(audioVolume);
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

