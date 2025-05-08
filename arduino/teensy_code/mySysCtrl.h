#include <ctime>
#include <cstdlib>
#include "WireIMXRT.h"
#include "core_pins.h"
/**
 * mySysCtrl.h - System Control Library
 * 
 * Functions for system state, timing, player identification, audio playback,
 * serial communication and system reports.
 * 
 * Created: 2025-03-20
 * Author: Antoine "Arthur" Hureau-Parreira
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
extern AudioPlaySdWav wavPlayer;     // Audio player reference
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
extern const int STARTUP_DELAY;
extern int pwmFreq;
const int CHECK_INTERVAL = 60000;

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


/**
 * Prints current date/time from RTC to Serial
 * Format: YYYY/MM/DD (DayName) HH:MM:SS
 */
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
 * Formats time in milliseconds to "mn:s:ms" string
 * @param ms Time in milliseconds
 * @return Formatted string
 */
String formatTimeToMinutesSecondsMs(unsigned long ms) {
  // Calculate minutes, seconds, and remaining milliseconds
  unsigned long minutes = ms / 60000;
  unsigned long seconds = (ms % 60000) / 1000;
  unsigned long milliseconds = ms % 1000;
  
  // Create the formatted string
  char buffer[15];
  
  // Add safety check
  if (sprintf(buffer, "%lu:%02lu:%03lu", (unsigned long)minutes, (unsigned long)seconds, (unsigned long)milliseconds) > 0 && strlen(buffer) < sizeof(buffer)) {
    buffer[sizeof(buffer) - 1] = '\0';
  } else {
    buffer[0] = '0';
    buffer[1] = '\0';
  }
  
  return String(buffer);
}

/**
 * Generates system report to Serial console
 * @param player The player ID for the report
 */
void systemReport(int player) {
  //header
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
  uint32_t trackLengthMs = wavPlayer.lengthMillis();
  uint32_t trackPositionMs = wavPlayer.positionMillis();
  if (trackLengthMs > 0 && trackPositionMs > 0){
    Serial.print("Track position ");
    Serial.print(formatTimeToMinutesSecondsMs(trackPositionMs));
    Serial.print(" / ");
    Serial.println(formatTimeToMinutesSecondsMs(trackLengthMs));
  }

  //temp
  float temp = tempmonGetTemp();
  Serial.print("CPU temperature: ");
  Serial.print(temp);
  Serial.println(" °C");

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
  //Serial.print("Audio Memory: ");
  //Serial.println(audioMemory);
  Serial.print("Startup Delay: ");
  Serial.print(STARTUP_DELAY);
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

  // System state
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
 * Powers on amplifier then speaker with delay, wakes system up
 */

void startupSequence() {
  //startup only if system is asleep
  if (!systemAwake){
    digitalWrite(REL_1, HIGH);  //turns amp on
    Serial.println("amp is ON");
    delay(REL_SW_DELAY);
    digitalWrite(REL_2, HIGH);  //turns speaker on
    Serial.println("speaker is ON");
    delay(REL_SW_DELAY);

    systemAwake = true; //system wakeup
    trackIteration = 0;
  }
}

/**
 * Powers off speaker then amplifier with delay, puts system to sleep
 */
void shutDownSequence() {
  //shutdown only if system is awake
  if (systemAwake){
    //stop any audio or light
    wavPlayer.stop();
    digitalWrite(PWM_PIN, LOW);

    digitalWrite(REL_2, LOW);  //turns speaker off
    Serial.println("speaker is OFF");
    delay(REL_SW_DELAY);
    digitalWrite(REL_1, LOW);  //turns amp off
    Serial.println("amp is OFF");
    delay(REL_SW_DELAY);

    systemAwake = false; //puts system asleep
  }
}

/**
 * Plays audio file and updates tracking data
 * Increments trackIteration and sets playbackStatus
 */
void playAudio() {
  wavPlayer.play(FILE_NAME);
  delay(50); //debounce
  trackIteration += 1;
  playbackStatus = true;
  
  Serial.print("Start playing ");
  Serial.println(FILE_NAME);
  Serial.print("Track iteration nr ");
  Serial.print(trackIteration);
  Serial.println(" during curent session (will be deleted tomorrow morning at 6AM).");

  float temp = tempmonGetTemp();
  Serial.print("CPU temperature: ");
  Serial.print(temp);
  Serial.println(" °C");

  if (PLAYER_ID == 0){
    clockMe();
  }
}

/**
 * Sends formatted single character commands via Serial3
 * @param command Character command to send
 */
void sendSerialCommand(char command) {
  Serial3.write(command);

  Serial.print("Command '");
  Serial.print(command);
  Serial.println("' was sent on Serial3");
  delay(50); //debounce
}

/**
 * Updates system state based on time of day
 * For LONG player, manages wake/sleep state based on configured hours
 * For SMALL and SEASHELL, it only controls playback status
 */
void statusUpdates() {
  //check static variables
  static unsigned long lastCheck = 0;
  static bool lastActiveState = false;
  static bool initialCheckDone = false;
  
  // check first 5 seconds after setup, then every minute
  unsigned long checkInterval = initialCheckDone ? CHECK_INTERVAL : STARTUP_DELAY;
  
  //if current time minus last check time is greater than interval, it's time to check status
  if (PLAYER_ID == 0 && (millis() - lastCheck > checkInterval)) {
    lastCheck = millis();
    initialCheckDone = true;
    DateTime now = rtc.now();
    int currentHour = now.hour();

    /*   
    Serial.print("Time check: ");
    Serial.print(currentHour);
    Serial.print(":");
    Serial.print(now.minute());
    Serial.print(" - Active hours: ");
    Serial.print(START_HOUR);
    Serial.print("-");
    Serial.println(END_HOUR);
    */
    
    //if current time is greater or equal to 6AM and inferior than 11PM, system should be active
    bool isActive = (currentHour >= START_HOUR && currentHour < END_HOUR);
    
    if (isActive != lastActiveState) {
      lastActiveState = isActive;
      
      //if system is active but asleep, trigger startup sequence
      if (isActive) {
        if (!systemAwake) {
          Serial.println("Entering active hours");
          sendSerialCommand(CMD_WAKEUP);
          startupSequence();
          displayBinaryCode(15);
        }
      } else { //if system is inactive but awake, trigger shutdown sequence
        if (systemAwake) {
          Serial.println("Exiting active hours");
          sendSerialCommand(CMD_SLEEP);
          shutDownSequence();
        }
      }
    }
  }
  
  // Update playback status
  if (!wavPlayer.isPlaying()) {
    playbackStatus = false;
  }
}

/**
 * Identifies player type and sets configuration
 * Sets PLAYER_ID (0=LONG, 1=SMALL, 2=SEASHELL) and audio file
 */
void setupPlayerID() {
  pinMode(SMALL_PIN, INPUT);
  pinMode(SEASHELL_PIN, INPUT);
  pinMode(LONG_PIN, INPUT);

  //read pin to define player ID
  if (digitalRead(LONG_PIN) == HIGH) {
    PLAYER_ID = 0;
  } else if (digitalRead(SMALL_PIN) == HIGH) {
    PLAYER_ID = 1;
  } else if (digitalRead(SEASHELL_PIN) == HIGH) {
    PLAYER_ID = 2;
  }

  Serial.print("Player ID is  ");
  Serial.println(PLAYER_ID);

  //match file name according to player ID
  if (PLAYER_ID == 0) {
    strcpy(FILE_NAME, LO_STR);
  } else if (PLAYER_ID == 1) {
    strcpy(FILE_NAME, SM_STR);
  } else if (PLAYER_ID == 2) {
    strcpy(FILE_NAME, SS_STR);
  }

  Serial.print("Audio file setup ");
  Serial.println(FILE_NAME);
}

/**
 * Processes single-character commands
 * @param cmd Command character
 * @return True if command processed successfully
 */
bool processCommand(char cmd) {
  switch(cmd) {
    case CMD_HELP:
      Serial.println("\n----- AVAILABLE COMMANDS -----");
      Serial.println("h - Display this help message");
      Serial.println("r - Generate system report");
      Serial.println("w - Wake up system");
      Serial.println("s - Put system to sleep");
      Serial.println("p - Play audio");
      Serial.println("! - Stop audio");
      Serial.println("z - Replay audio");
      Serial.println("+ - Increase volume");
      Serial.println("- - Decrease volume");
      Serial.println("> - Increase PWM range");
      Serial.println("< - Decrease PWM range");
      Serial.println("1-4 - Toggle individual LEDs");
      Serial.println("------------------------------\n");
      return true;
      
    case CMD_REPORT:
      Serial.println("Generating system report...");
      systemReport(PLAYER_ID);
      return true;
      
    case CMD_WAKEUP:
      if (!systemAwake) {
        startupSequence();
        Serial.println("System woken up");
      }
      return true;
      
    case CMD_SLEEP:
      if (systemAwake) {
        shutDownSequence();
        Serial.println("System going to sleep");
      }
      return true;
      
    case CMD_PLAY:
      playAudio();
      Serial.println("Playing audio");
      return true;
      
    case CMD_REPLAY:
      wavPlayer.stop();
      playAudio();
      Serial.println("Replay command, resetting playback");
      return true;
      
    case CMD_STOP:
      wavPlayer.stop();
      Serial.println("Stopping audio");
      return true;
      
    case CMD_VOL_UP:
      audioVolume += 0.1f;
      if (audioVolume > 1.0f) audioVolume = 1.0f;
      sgtl5000.volume(audioVolume);
      Serial.print("Volume increased to: ");
      Serial.println(audioVolume);
      return true;
      
    case CMD_VOL_DOWN:
      audioVolume -= 0.1f;
      if (audioVolume < 0.0f) audioVolume = 0.0f;
      sgtl5000.volume(audioVolume);
      Serial.print("Volume decreased to: ");
      Serial.println(audioVolume);
      return true;
      
    case CMD_PWM_UP:
      rangePWM += 25;
      if (rangePWM > 255) rangePWM = 255;
      Serial.print("PWM range increased to: ");
      Serial.println(rangePWM);
      return true;
      
    case CMD_PWM_DOWN:
      rangePWM -= 25;
      if (rangePWM < 0) rangePWM = 0;
      Serial.print("PWM range decreased to: ");
      Serial.println(rangePWM);
      return true;
      
    case CMD_LED_1: {
      // Add braces to create a scope for the variable
      int ledIndex = 0;
      digitalWrite(LED_ARRAY[ledIndex], !digitalRead(LED_ARRAY[ledIndex]));
      Serial.print("Toggled LED 1");
      return true;
    }
    
    case CMD_LED_2: {
      int ledIndex = 1;
      digitalWrite(LED_ARRAY[ledIndex], !digitalRead(LED_ARRAY[ledIndex]));
      Serial.print("Toggled LED 2");
      return true;
    }
    
    case CMD_LED_3: {
      int ledIndex = 2;
      digitalWrite(LED_ARRAY[ledIndex], !digitalRead(LED_ARRAY[ledIndex]));
      Serial.print("Toggled LED 3");
      return true;
    }
    
    case CMD_LED_4: {
      int ledIndex = 3;
      digitalWrite(LED_ARRAY[ledIndex], !digitalRead(LED_ARRAY[ledIndex]));
      Serial.print("Toggled LED 4");
      return true;
    }
      
    default:
      Serial.print("Unknown command: ");
      Serial.println(cmd);
      Serial.println("Type 'h' for available commands");
      return false;
  }
  
  return false;
}

/**
 * Processes commands from USB Serial
 * @return True if command was processed
 */
bool checkUsbCommands() {
  bool commandProcessed = false;
  
  if (Serial.available() > 0) {
    // Read a single character
    char inChar = (char)Serial.read();
    
    // Skip whitespace and control characters
    if (inChar > 32) {  // Non-whitespace, non-control character
      // Display the received command
      Serial.print("USB command received: '");
      Serial.print(inChar);
      Serial.println("'");

      // If this is the leader, relay the command to followers
      if (PLAYER_ID == 0) {
        sendSerialCommand(inChar);
      }
      
      // Process the single-character command
      switch(inChar) {
        case CMD_LED_1:
        case CMD_LED_2:
        case CMD_LED_3:
        case CMD_LED_4:
        case CMD_HELP:
        case CMD_WAKEUP:
        case CMD_PLAY:
        case CMD_SLEEP:
        case CMD_STOP:
        case CMD_REPLAY:
        case CMD_REPORT:
        case CMD_VOL_UP:
        case CMD_VOL_DOWN:
        case CMD_PWM_UP:
        case CMD_PWM_DOWN:
          // Valid command - process it
          commandProcessed = processCommand(inChar);
          break;
          
        default:
          // Invalid command
          Serial.print("Unknown command: '");
          Serial.print(inChar);
          Serial.println("'");
          break;
      }
    }
    
    // Consume any extra characters in the buffer (like newlines)
    while (Serial.available() > 0) {
      Serial.read();
    }
  }
  
  return commandProcessed;
}

#endif // MYSYSCTRL_H