/**
 * mySysCtrl.h - System Control Library
 * 
 * This library provides functions to manage the system state, player identification,
 * and reporting for the Teensy-based sound-to-light installation.
 * 
 * Created: 2024-04-18
 * Author: Antoine "Arthur" Hureau-Parreira
 * 
 * Ant Art Enk, Bergen NO
 */

#ifndef MYSYSCTRL_H
#define MYSYSCTRL_H

#include <Arduino.h>
#include <RTClib.h>

// External references to variables defined in the main program
extern int PLAYER_ID;            // Current player ID (0=LONG, 1=SMALL, 2=SEASHELL)
extern char FILE_NAME[];         // Current audio file name
extern bool systemAwake;         // System active state
extern bool playbackStatus;      // Audio playback state
extern int trackIteration;       // Track play count for current day

// Hardware
extern RTC_DS3231 rtc;           // RTC module reference
extern AudioPlaySdWav sdWav;     // Audio player reference
extern AudioControlSGTL5000 sgtl5000; //audio control reference

// External pin references
extern const int SMALL_PIN;      // Pin for SMALL player identification
extern const int SEASHELL_PIN;   // Pin for SEASHELL player identification
extern const int LONG_PIN;       // Pin for LONG player identification

// External constant references
extern const char SM_STR[];      // File name for SMALL player
extern const char SS_STR[];      // File name for SEASHELL player
extern const char LO_STR[];      // File name for LONG player
extern const int START_HOUR;     // Daily wake-up hour
extern const int END_HOUR;       // Daily sleep hour
extern const bool PEAK_MODE;     // Audio peak vs RMS mode
extern const char days[][12];    // Weekday names array
extern const int REL_SW_DELAY;   // Delay between relay switching operations

// External references for system report
extern const int SDCARD_CS_PIN;
extern const int SDCARD_MOSI_PIN;
extern const int SDCARD_SCK_PIN;
extern const int REL_1;
extern const int REL_2;
extern const int LED_1;
extern const int LED_2;
extern const int LED_3;
extern const int LED_4;
extern const int PWM_PIN;
extern const uint8_t VOL_CTRL_PIN;
extern const uint8_t LED_ARRAY[];
extern float audioVolume;
extern int rangePWM;
extern int currentCode;
extern int audioMemory;
extern int startupDelay;
extern int pwmFreq;

//helper function to print time
void clockMe() {
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

/**
 * Formats time in milliseconds to a string in the format "mn:s:ms"
 * 
 * @param ms Time in milliseconds
 * @return String formatted as minutes:seconds:milliseconds
 */
String formatTimeToMinutesSecondsMs(uint32_t ms) {
  // Calculate minutes, seconds, and remaining milliseconds
  uint32_t minutes = ms / 60000;
  uint32_t seconds = (ms % 60000) / 1000;
  uint32_t milliseconds = ms % 1000;
  
  // Create the formatted string
  char buffer[15]; // Enough space for "999:59:999" plus null terminator
  sprintf(buffer, "%u:%02u:%03u", minutes, seconds, milliseconds);
  
  return String(buffer);
}

/**
 * Generates a comprehensive system report to the Serial console
 * 
 * This function outputs detailed information about the current system 
 * configuration, including:
 * - Player ID and configuration
 * - Pin assignments for SD card, digital pins, and analog pins
 * - LED array configuration
 * - System settings (volume, PWM range, audio memory, etc.)
 * - Current system states (awake/sleep, playback, peak mode)
 * - Date/time information
 * - Audio file names
 * 
 * @param player The player ID to include in the report
 * 
 * @note This function is useful for debugging and system verification
 */
void systemReport(int player) {
  Serial.println("\n----- SYSTEM REPORT -----");
  if (PLAYER_ID == 0){
    Serial.print("RTC time: ");
    clockMe();
  }

  //player ID an file
  Serial.print("Player ID: ");
  Serial.println(player);
  Serial.print("Current file: ");
  Serial.println(FILE_NAME);

  // Track length and position
  uint32_t trackLengthMs = sdWav.lengthMillis();
  uint32_t trackPositionMs = sdWav.positionMillis();
  if (trackLengthMs > 0 && trackPositionMs > 0){
    Serial.print("Track position ");
    Serial.print(formatTimeToMinutesSecondsMs(trackPositionMs));
    Serial.print(" / ");
    Serial.println(formatTimeToMinutesSecondsMs(trackLengthMs));
  }


  // SD CARD configuration
  Serial.println("\n-- SD CARD PINS --");
  Serial.print("CS: ");
  Serial.println(SDCARD_CS_PIN);
  Serial.print("MOSI: ");
  Serial.println(SDCARD_MOSI_PIN);
  Serial.print("SCK: ");
  Serial.println(SDCARD_SCK_PIN);

  // Digital pins
  Serial.println("\n-- DIGITAL PINS --");
  Serial.print("REL_1: ");
  Serial.println(REL_1);
  Serial.print("REL_2: ");
  Serial.println(REL_2);
  Serial.print("LED_1: ");
  Serial.println(LED_1);
  Serial.print("LED_2: ");
  Serial.println(LED_2);
  Serial.print("LED_3: ");
  Serial.println(LED_3);
  Serial.print("LED_4: ");
  Serial.println(LED_4);
  Serial.print("PWM_PIN: ");
  Serial.println(PWM_PIN);
  Serial.print("SMALL_PIN: ");
  Serial.println(SMALL_PIN);
  Serial.print("SEASHELL_PIN: ");
  Serial.println(SEASHELL_PIN);
  Serial.print("LONG_PIN: ");
  Serial.println(LONG_PIN);

  // Analog pins
  Serial.println("\n-- ANALOG PINS --");
  Serial.print("VOL_CTRL_PIN: ");
  Serial.println(VOL_CTRL_PIN);

  // LED array
  Serial.println("\n-- LED ARRAY --");
  for (int i = 0; i < 4; i++) {
    Serial.print("LED_ARRAY[");
    Serial.print(i);
    Serial.print("]: ");
    Serial.println(LED_ARRAY[i]);
  }

  // System settings
  Serial.println("\n-- SYSTEM SETTINGS --");
  Serial.print("Audio Volume: ");
  Serial.println(audioVolume);
  Serial.print("PWM Range: ");
  Serial.println(rangePWM);
  Serial.print("Current Code: ");
  Serial.println(currentCode);
  Serial.print("Audio Memory: ");
  Serial.println(audioMemory);
  Serial.print("Startup Delay: ");
  Serial.print(startupDelay);
  Serial.println(" ms");
  Serial.print("Track Iteration: ");
  Serial.println(trackIteration);
  Serial.print("Start Hour: ");
  Serial.println(START_HOUR);
  Serial.print("End Hour: ");
  Serial.println(END_HOUR);
  Serial.print("PWM Frequency: ");
  Serial.print(pwmFreq);
  Serial.println(" Hz");

  // System states
  Serial.println("\n-- SYSTEM STATES --");
  Serial.print("System Awake: ");
  Serial.println(systemAwake ? "YES" : "NO");
  Serial.print("Playback Status: ");
  Serial.println(playbackStatus ? "PLAYING" : "STOPPED");
  Serial.print("Peak Mode: ");
  Serial.println(PEAK_MODE ? "ENABLED" : "DISABLED");

  Serial.println("\n----- END REPORT -----\n");
}

/**
 * Performs a sequenced startup of the amplifier and speaker
 * 
 * This function powers on the system components in the correct order:
 * 1. Turns on the amplifier (REL_1)
 * 2. Waits for REL_SW_DELAY milliseconds
 * 3. Turns on the speaker (REL_2)
 * 4. Waits for REL_SW_DELAY milliseconds
 * 
 * The sequence prevents power surges and potential speaker damage
 * by ensuring the amplifier is powered before the speaker.
 */
void startupSequence() {
  digitalWrite(REL_1, HIGH);  //turns amp on
  Serial.println("amp is ON");
  delay(REL_SW_DELAY);
  digitalWrite(REL_2, HIGH);  //turns speaker on
  Serial.println("speaker is ON");
  delay(REL_SW_DELAY);
}

/**
 * Performs a sequenced shutdown of the speaker and amplifier
 * 
 * This function powers off the system components in the correct order,
 * but only if audio is not currently playing:
 * 1. Checks if audio is playing (sdWav.isPlaying())
 * 2. If not playing, turns off the speaker (REL_2)
 * 3. Waits for REL_SW_DELAY milliseconds
 * 4. Turns off the amplifier (REL_1)
 * 5. Waits for REL_SW_DELAY milliseconds
 * 
 * The sequence prevents audio cutoff during playback and potential
 * speaker damage by ensuring the speaker is powered off before the amplifier.
 * 
 * @note If audio is currently playing, this function does nothing
 */
void shutDownSequence() {
  if (!sdWav.isPlaying()) {
    digitalWrite(REL_2, LOW);  //turns speaker off
    Serial.println("speaker is OFF");
    delay(REL_SW_DELAY);
    digitalWrite(REL_1, LOW);  //turns amp off
    Serial.println("amp is OFF");
    delay(REL_SW_DELAY);
  }
}

/**
 * Updates the system state based on time of day
 * 
 * For the LONG player (PLAYER_ID == 0), this function checks the current time
 * from the RTC and manages the system's awake/sleep state according to the
 * configured START_HOUR and END_HOUR. It handles:
 * - Waking up the system (calling startupSequence()) when entering active hours
 * - Putting the system to sleep (calling shutDownSequence()) when exiting active hours
 * - Resetting track iteration count at the start of a new active period
 * 
 * @note This function has no effect on SMALL or SEASHELL players (PLAYER_ID != 0)
 */
void statusUpdates() {
  if (PLAYER_ID == 0) {
    if (rtc.now().hour() >= START_HOUR && rtc.now().hour() < END_HOUR) {
      if (!systemAwake) {
        startupSequence();
        systemAwake = !systemAwake;
        trackIteration = 0;
      }
    } else {
      if (systemAwake) {
        shutDownSequence();
        systemAwake = !systemAwake;
      }
    }
  }
  if (!sdWav.isPlaying()) {
    playbackStatus = false;
  }
}

/**
 * Identifies the player type and sets up related configuration
 * 
 * This function determines which player type the device is (LONG, SMALL, or SEASHELL)
 * by checking specific input pins. Based on the identified player type, it:
 * - Sets the PLAYER_ID value (0=LONG, 1=SMALL, 2=SEASHELL)
 * - Configures the correct audio file name for the player type
 * 
 * @note This function should be called during system initialization
 * 
 * Pin-to-player mapping:
 * - HIGH on LONG_PIN (pin 32) for LONG player (ID 0)
 * - HIGH on SMALL_PIN (pin 30) for SMALL player (ID 1)
 * - HIGH on SEASHELL_PIN (pin 28) for SEASHELL player (ID 2)
 */
void setupPlayerID() {
  pinMode(SMALL_PIN, INPUT);
  pinMode(SEASHELL_PIN, INPUT);
  pinMode(LONG_PIN, INPUT);

  if (digitalRead(SMALL_PIN) == HIGH) {
    PLAYER_ID = 1;
    strcpy(FILE_NAME, SM_STR);
  }

  if (digitalRead(SEASHELL_PIN) == HIGH) {
    PLAYER_ID = 2;
    strcpy(FILE_NAME, SS_STR);
  }

  if (digitalRead(LONG_PIN) == HIGH) {
    PLAYER_ID = 0;
    strcpy(FILE_NAME, LO_STR);
  }

  Serial.print("Audio file setup ");
  Serial.println(FILE_NAME);
}

/**
 * Checks for and processes commands received from the Serial monitor
 * 
 * This function should be called regularly in the main loop to check for
 * incoming serial commands. Current supported commands:
 * - "report" or "r": Triggers a system report
 * - "help" or "h": Displays available commands
 * - "v[value]" (e.g. "v0.7"): Sets volume (0.0 to 1.0 range)
 * 
 * @return true if a command was processed, false otherwise
 */
bool checkSerialCommands() {
  static String inputBuffer = "";
  bool commandProcessed = false;
  
  // Process any available Serial data
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    
    // Add to buffer if not a line ending
    if (inChar != '\n' && inChar != '\r') {
      inputBuffer += inChar;
    }
    // Process command on line ending
    else if (inputBuffer.length() > 0) {
      // Convert to lowercase for case-insensitive comparison
      inputBuffer.toLowerCase();
      inputBuffer.trim(); // Remove any whitespace
      
      // Check for report command
      if (inputBuffer == "report") {
        Serial.println("Generating system report...");
        systemReport(PLAYER_ID);
        commandProcessed = true;
      }
      // Check for help command
      else if (inputBuffer == "help" || inputBuffer == "h") {
        Serial.println("\n----- AVAILABLE COMMANDS -----");
        Serial.println("report (r) - Generate system report");
        Serial.println("help (h) - Display this help message");
        Serial.println("v[0.0-1.0] - Set volume (e.g. v0.7)");
        Serial.println("------------------------------\n");
        commandProcessed = true;
      }
      //check for replay command
      else if (inputBuffer == "replay") {
        sdWav.stop();
        Serial.print("replay command, ");
        commandProcessed = true;
      }
      //check pwm mod command
      else if (inputBuffer.startsWith("pwm")) {
        String pwmStr = inputBuffer.substring(3);
        float pwm = pwmStr.toFloat() * 255.;
        if (pwm >= 0 && pwm <= 255){
          rangePWM = pwm;
          Serial.print("PWM range changed to:  0.00 - ");
          Serial.println(rangePWM);
          commandProcessed = true;
        } else{
          Serial.println("Invalid PWM range. Please use a value between 0.0 and 1.0");
          commandProcessed = true;
        }
      }
      //check for PWM frequence
      else if (inputBuffer.startsWith("freq")) {
        String freqStr = inputBuffer.substring(4);
        int freq = freqStr.toInt();
        if (freq >= 0 && freq <= 100){
          pwmFreq = freq;
          Serial.print("PWM frequence changed to:  ");
          Serial.println(pwmFreq);
          commandProcessed = true;
        } else{
          Serial.println("Invalid PWM freq. Please use value between 0.0 and 100");
          commandProcessed = true;
        }
      }
      // Check for volume mod command
      else if (inputBuffer.startsWith("vol")) {
        String volumeStr;
        if (inputBuffer.startsWith("vol")) {
          volumeStr = inputBuffer.substring(3);
        }
        float volume = volumeStr.toFloat();
        
        // Validate the volume range
        if (volume >= 0.0 && volume <= 1.0) {
          Serial.print("Audio volume changed to: ");
          Serial.println(volume);
          sgtl5000.volume(volume);
          audioVolume = volume;
          commandProcessed = true;
        } else {
          Serial.println("Invalid volume. Please use a value between 0.0 and 1.0");
          commandProcessed = true;
        }
      }
      // Unknown command
      else {
        Serial.print("Unknown command: ");
        Serial.println(inputBuffer);
        Serial.println("Type 'help' for available commands");
        commandProcessed = true;
      }
      
      // Clear buffer for next command
      inputBuffer = "";
    }
  }
  
  return commandProcessed;
}

#endif // MYSYSCTRL_H