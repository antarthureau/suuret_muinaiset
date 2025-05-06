#include "WireIMXRT.h"
#include "core_pins.h"
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
//extern int audioMemory;
extern const int STARTUP_DELAY;
extern int pwmFreq;

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
 * Prints the current date and time from the RTC to the Serial monitor
 * 
 * This helper function retrieves the current timestamp from the real-time clock (RTC)
 * and formats it in a human-readable format, then outputs it to the Serial monitor.
 * The output format is: YYYY/MM/DD (DayName) HH:MM:SS
 * 
 * Example output: 2025/04/29 (Tuesday) 14:30:22
 * 
 * Dependencies:
 *   - Requires an initialized RTC object named 'rtc'
 *   - Requires a string array 'days' containing day names indexed by dayOfTheWeek()
 *     (where Sunday=0, Monday=1, etc.)
 *   - Requires the DateTime class from an RTC library (typically RTClib)
 * 
 * This function doesn't take any parameters or return any values.
 * It only outputs the formatted time to Serial.
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
 * Formats time in milliseconds to a string in the format "mn:s:ms"
 * 
 * @param ms Time in milliseconds
 * @return String formatted as minutes:seconds:milliseconds
 */
String formatTimeToMinutesSecondsMs(unsigned long ms) {
  // Calculate minutes, seconds, and remaining milliseconds
  unsigned long minutes = ms / 60000;
  unsigned long seconds = (ms % 60000) / 1000;
  unsigned long milliseconds = ms % 1000;
  
  // Create the formatted string
  char buffer[15]; // Enough space for "999:59:999" plus null terminator
  
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
  uint32_t trackLengthMs = wavPlayer.lengthMillis();
  uint32_t trackPositionMs = wavPlayer.positionMillis();
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
  Serial.print("Audio Memory: 32");
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

  systemAwake = true;
}

/**
 * Performs a sequenced shutdown of the speaker and amplifier
 * 
 * This function powers off the system components in the correct order,
 * but only if audio is not currently playing:
 * 1. Checks if audio is playing (wavPlayer.isPlaying())
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
    wavPlayer.stop();
    digitalWrite(PWM_PIN, LOW);

    digitalWrite(REL_2, LOW);  //turns speaker off
    Serial.println("speaker is OFF");
    delay(REL_SW_DELAY);
    digitalWrite(REL_1, LOW);  //turns amp off
    Serial.println("amp is OFF");
    delay(REL_SW_DELAY);

    systemAwake = false;
}

/**
 * Plays an audio file and manages playback status and tracking
 * 
 * This function initiates playback of the audio file defined by FILE_NAME constant
 * using the wavPlayer object. It updates the tracking information for the current
 * session, including incrementing the playback iteration counter and setting the
 * playback status flag to true.
 * 
 * The function also provides feedback via Serial output about:
 *  - Which file is being played
 *  - The current iteration number within the session
 *  - A reminder about the daily reset of tracking data
 * 
 * Global variables affected:
 *  - trackIteration: Incremented to count total playbacks in the current session
 *  - playbackStatus: Set to true to indicate active playback
 * 
 * Dependencies:
 *  - Requires a properly initialized wavPlayer object
 *  - Requires the FILE_NAME constant to be defined with a valid audio file path
*/
void playAudio() {
  wavPlayer.play(FILE_NAME);
  delay(10);
  trackIteration += 1;
  playbackStatus = true;
  
  // Safety check - ensure FILE_NAME is null-terminated
  FILE_NAME[12] = '\0';
  
  Serial.print("Start playing ");
  Serial.println(FILE_NAME);
  Serial.print("Track iteration nr ");
  Serial.print(trackIteration);
  Serial.println(" during curent session (will be deleted tomorrow morning at 6AM).");
}

/**
 * Sends a formatted command to a receiving device via Serial3
 * 
 * This function takes a command string and sends it over Serial3 using the
 * required format ":command." (starting with a colon, ending with a period).
 * It automatically adds the required delimiters and appends a newline character
 * after the command for proper command parsing on the receiving end.
 * 
 * The function includes a short delay after sending to prevent command overlap
 * and ensure reliable transmission, especially when multiple commands are sent
 * in quick succession.
 * 
 * @param command The command string to send (without delimiters)
 *                This should be the raw command like "play", "stop", etc.
 *                Delimiters (: and .) will be added automatically
 */
void sendSerialCommand(char command) {
  // Print the command for debugging
  Serial.print("Sending command: '");
  Serial.print(command);
  Serial.println("'");
  
  // Send just the single character
  Serial3.write(command);
  
  Serial.print("Command '");
  Serial.print(command);
  Serial.println("' was sent on Serial3");
  
  delay(50); // Short delay to ensure transmission completes
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
    // Debug output once every 30 seconds
    static unsigned long lastDebugTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastDebugTime > 30000) {
      lastDebugTime = currentTime;
      
      // Get current time and print it
      DateTime now = rtc.now();
      Serial.print("RTC time check: ");
      Serial.print(now.hour());
      Serial.print(":");
      Serial.print(now.minute());
      Serial.print(":");
      Serial.print(now.second());
      Serial.print(" - In range: ");
      Serial.println((now.hour() >= START_HOUR && now.hour() < END_HOUR) ? "YES" : "NO");
    }
    
    // Only check if we're in active hours once per minute
    static unsigned long lastCheck = 0;
    static bool wasActive = false;
    
    if (currentTime - lastCheck > 60000) {
      lastCheck = currentTime;
      
      bool isActive = (rtc.now().hour() >= START_HOUR && rtc.now().hour() < END_HOUR);
      
      // Only act on changes
      if (isActive != wasActive) {
        Serial.print("Activity state changed from ");
        Serial.print(wasActive ? "ACTIVE" : "INACTIVE");
        Serial.print(" to ");
        Serial.println(isActive ? "ACTIVE" : "INACTIVE");
        
        if (isActive) {
          // Entering active period
          sendSerialCommand(CMD_WAKEUP);
          startupSequence();
          trackIteration = 0;
          displayBinaryCode(15);
        } else {
          // Exiting active period
          sendSerialCommand(CMD_SLEEP);
          shutDownSequence();
        }
        
        wasActive = isActive;
      }
    }
  }
  
  // Update playback status
  if (!wavPlayer.isPlaying() && playbackStatus) {
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
    // Ensure null termination (13 is the known size from declaration)
    FILE_NAME[12] = '\0';
  }

  if (digitalRead(SEASHELL_PIN) == HIGH) {
    PLAYER_ID = 2;
    strcpy(FILE_NAME, SS_STR);
    FILE_NAME[12] = '\0';
  }

  if (digitalRead(LONG_PIN) == HIGH) {
    PLAYER_ID = 0;
    strcpy(FILE_NAME, LO_STR);
    FILE_NAME[12] = '\0';
  }

  Serial.print("Audio file setup ");
  Serial.println(FILE_NAME);
}

/**
 * Processes commands for both USB and Serial3 interfaces
 * 
 * @param cmd Command string to process
 * @param cmdLength Length of the command string
 * @return True if command was processed successfully
 * 
 * Supports commands: h/help, r/report, w/wakeup, s/sleep, p/play,
 * z/replay, !/stop, vol[0.0-1.0], pwm[0.0-1.0], freq[0-100], led[0-15]
 */
bool processCommandComplex(char* cmd, int cmdLength) {
  // Safety check for valid parameters
  if (cmd == NULL || cmdLength <= 0 || cmdLength >= 32) {
    Serial.println("Invalid command parameters");
    return false;
  }
  
  // Ensure the buffer is properly null-terminated
  cmd[cmdLength] = '\0';
  
  // Convert to lowercase for case-insensitive comparison
  for (int i = 0; i < cmdLength && i < 31; i++) {
    cmd[i] = tolower(cmd[i]);
  }
  
  // HELP command
  if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
    Serial.println("\n----- AVAILABLE COMMANDS -----");
    Serial.println("help - Display this help message");
    Serial.println("report - Generate system report");
    Serial.println("replay - Resets file playback");
    Serial.println("vol[0.0-1.0] - Set volume (e.g. vol0.8)");
    Serial.println("pwm[0-255] - Set PWM range (e.g. pwm255)");
    Serial.println("freq[0-100] - Sets PWM reaction frequency (e.g. freq25)");
    Serial.println("------------------------------\n");
    return true;
  }
  
  // REPORT command
  else if (strcmp(cmd, "r") == 0 || strcmp(cmd, "report") == 0) {
    Serial.println("Generating system report...");
    systemReport(PLAYER_ID);
    return true;
  }
  
  // WAKEUP command
  else if (strcmp(cmd, "w") == 0 || strcmp(cmd, "wakeup") == 0) {
    if (!systemAwake) {
      startupSequence();
      systemAwake = true;
      Serial.println("System woken up");
    }
    return true;
  }
  
  // SLEEP command
  else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "sleep") == 0) {
    if (systemAwake) {
      systemAwake = false;
      shutDownSequence();
      Serial.println("System going to sleep");
    }
    return true;
  }
  
  // PLAY command
  else if (strcmp(cmd, "p") == 0 || strcmp(cmd, "play") == 0) {
    playAudio();
    Serial.println("Playing audio");
    return true;
  }
  
  // REPLAY command
  else if (strcmp(cmd, "z") == 0 || strcmp(cmd, "replay") == 0) {
    wavPlayer.stop();
    Serial.println("Replay command, resetting playback");
    if (PLAYER_ID == 0) {
      sendSerialCommand(CMD_PLAY);
    }
    return true;
  }
  
  // STOP command
  else if (strcmp(cmd, "!") == 0 || strcmp(cmd, "stop") == 0) {
    wavPlayer.stop();
    Serial.println("Stopping audio");
    return true;
  }
  
  // VOLUME command (vol0.x)
  else if (strncmp(cmd, "vol", 3) == 0) {
    float volume = atof(cmd + 3);
    
    if (volume >= 0.0 && volume <= 1.0) {
      Serial.print("Audio volume changed to: ");
      Serial.println(volume);
      sgtl5000.volume(volume);
      audioVolume = volume;
      return true;
    } else {
      Serial.println("Invalid volume. Please use a value between 0.0 and 1.0");
      return true;
    }
  }
  
  // PWM RANGE command (pwmx.xx)
  else if (strncmp(cmd, "pwm", 3) == 0) {
    float pwm = atof(cmd + 3) * 255.0;
    
    if (pwm >= 0 && pwm <= 255) {
      rangePWM = pwm;
      Serial.print("PWM range changed to:  0.00 - ");
      Serial.println(rangePWM);
      return true;
    } else {
      Serial.println("Invalid PWM range. Please use a value between 0.0 and 1.0");
      return true;
    }
  }
  
  // PWM FREQ command (freqxx)
  else if (strncmp(cmd, "freq", 4) == 0) {
    int freq = atoi(cmd + 4);
    
    if (freq >= 0 && freq <= 100) {
      pwmFreq = freq;
      Serial.print("PWM frequency changed to: ");
      Serial.println(pwmFreq);
      return true;
    } else {
      Serial.println("Invalid PWM frequency. Please use value between 0 and 100");
      return true;
    }
  }
  
  // LED command (ledxx)
  else if (strncmp(cmd, "led", 3) == 0) {
    int ledValue = atoi(cmd + 3);
    
    if (ledValue >= 0 && ledValue <= 15) {
      displayBinaryCode(ledValue);
      return true;
    } else {
      Serial.println("Invalid LED value. Must be between 0 and 15");
      return true;
    }
  }
  
  // UNKNOWN command
  else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
    Serial.println("Type 'help' for available commands");
    return true;
  }
  
  return false;
}

/**
 * Processes single-character commands
 * 
 * @param cmd The command character to process
 * @return True if command was processed successfully
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
      Serial.println("z - Reset and replay audio");
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
        systemAwake = true;
        Serial.println("System woken up");
      }
      return true;
      
    case CMD_SLEEP:
      if (systemAwake) {
        systemAwake = false;
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
      Serial.println("Replay command, resetting playback");
      if (PLAYER_ID == 0) {
        sendSerialCommand(CMD_PLAY);
      }
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
  
  // Add a return statement at the end to avoid warning
  return false;
}

/**
 * Processes commands from USB Serial interface (Serial monitor)
 * Simplified for single-character command handling
 * 
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
          
          // If this is the leader, relay the command to followers
          if (PLAYER_ID == 0) {
            sendSerialCommand(inChar);
          }
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