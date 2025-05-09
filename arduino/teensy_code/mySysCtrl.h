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
extern bool messageIncoming;
extern const int MSG_BUFFER_SIZE;
extern char messageBuffer[];

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
#define MSG_REQUEST_SMALL "small"
#define MSG_REQUEST_SEASHELL "seashell"
#define MSG_HELP ":help"

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
    Serial.print("RTC time ");
    clockMe();
  }

  //player ID an file
  Serial.print("Player ID ");
  Serial.println(player);
  Serial.print("Current file ");
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
  Serial.print("CPU temperature ");
  Serial.print(temp);
  Serial.println(" °C");

  // SD CARD configuration
  Serial.println("\n-- SD CARD PINS --");
  Serial.print("CS ");
  Serial.println(SDCARD_CS_PIN);
  Serial.print("MOSI ");
  Serial.println(SDCARD_MOSI_PIN);
  Serial.print("SCK ");
  Serial.println(SDCARD_SCK_PIN);

  // Digital pins
  Serial.println("\n-- DIGITAL PINS --");
  Serial.print("REL_1 ");
  Serial.println(REL_1);
  Serial.print("REL_2 ");
  Serial.println(REL_2);
  Serial.print("LED_1 ");
  Serial.println(LED_1);
  Serial.print("LED_2 ");
  Serial.println(LED_2);
  Serial.print("LED_3 ");
  Serial.println(LED_3);
  Serial.print("LED_4 ");
  Serial.println(LED_4);
  Serial.print("PWM_PIN ");
  Serial.println(PWM_PIN);
  Serial.print("SMALL_PIN ");
  Serial.println(SMALL_PIN);
  Serial.print("SEASHELL_PIN ");
  Serial.println(SEASHELL_PIN);
  Serial.print("LONG_PIN ");
  Serial.println(LONG_PIN);

  // Analog pins
  Serial.println("\n-- ANALOG PINS --");
  Serial.print("VOL_CTRL_PIN ");
  Serial.println(VOL_CTRL_PIN);

  // LED array
  Serial.println("\n-- LED ARRAY --");
  for (int i = 0; i < 4; i++) {
    Serial.print("LED_ARRAY[");
    Serial.print(i);
    Serial.print("] ");
    Serial.println(LED_ARRAY[i]);
  }

  // System settings
  Serial.println("\n-- SYSTEM SETTINGS --");
  Serial.print("Audio Volume ");
  Serial.println(audioVolume);
  Serial.print("PWM Range ");
  Serial.println(rangePWM);
  Serial.print("Current Code ");
  Serial.println(currentCode);
  //Serial.print("Audio Memory ");
  //Serial.println(audioMemory);
  Serial.print("Startup Delay ");
  Serial.print(STARTUP_DELAY);
  Serial.println(" ms");
  Serial.print("Track Iteration ");
  Serial.println(trackIteration);
  Serial.print("Start Hour ");
  Serial.println(START_HOUR);
  Serial.print("End Hour ");
  Serial.println(END_HOUR);
  Serial.print("PWM Frequency ");
  Serial.print(pwmFreq);
  Serial.println(" Hz");

  // System state
  Serial.println("\n-- SYSTEM STATES --");
  Serial.print("System Awake ");
  Serial.println(systemAwake ? "YES" : "NO");
  Serial.print("Playback Status ");
  Serial.println(playbackStatus ? "PLAYING" : "STOPPED");
  Serial.print("Peak Mode ");
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
  Serial.print("CPU temperature ");
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

void sendSerialMessage(char* message){
  Serial3.write(":"); //means a message (string) is incoming
  Serial3.write(message);

  Serial.print("Message '");
  Serial.print(message);
  Serial.println("' was sent on Serial3");
  delay(50); //debounce
}

/**
 * Sends status information back to the leader via Serial3
 * Only used by followers (PLAYER_ID != 0)
 */
void sendStatusToLeader() {
  // Only followers should send status
  if (PLAYER_ID == 0) return;
  
  // Create a status message
  char statusMsg[MSG_BUFFER_SIZE];
  
  // Get CPU temperature
  float temp = tempmonGetTemp();
  
  // Get audio playback position if playing
  uint32_t positionMs = 0;
  uint32_t lengthMs = 0;
  if (wavPlayer.isPlaying()) {
    positionMs = wavPlayer.positionMillis();
    lengthMs = wavPlayer.lengthMillis();
  }
  
  // Format the status message - :STATUS|PLAYERID|TEMP|AWAKE|PLAYING|POS|LEN
  snprintf(statusMsg, MSG_BUFFER_SIZE, ":STATUS|%d|%.1f|%d|%d|%lu|%lu", 
          PLAYER_ID,              // Player ID
          temp,                   // CPU temperature
          systemAwake ? 1 : 0,    // System awake status
          playbackStatus ? 1 : 0, // Playback status
          positionMs,             // Current position in ms
          lengthMs                // Total length in ms
         );
  
  // Send the status message to leader
  Serial3.println(statusMsg);
  
  Serial.print("Sent status to leader: ");
  Serial.println(statusMsg);
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
      Serial.println("h - :help Display this help message");
      Serial.println("r - :report Generate system report");
      Serial.println("w - :wakeup Wake up system");
      Serial.println("s - :sleep Put system to sleep");
      Serial.println("p - :play Play audio");
      Serial.println("! - :stop Stop audio");
      Serial.println("z - :replay Replay audio");
      Serial.println("+ - :volup Increase volume");
      Serial.println("- - :voldown Decrease volume");
      Serial.println("> - :pwmupIncrease PWM range");
      Serial.println("< - :pwmdown Decrease PWM range");
      Serial.println("1-4 - :ledx Toggle individual LEDs");
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
      Serial.print("Volume increased to ");
      Serial.println(audioVolume);
      return true;
      
    case CMD_VOL_DOWN:
      audioVolume -= 0.1f;
      if (audioVolume < 0.0f) audioVolume = 0.0f;
      sgtl5000.volume(audioVolume);
      Serial.print("Volume decreased to ");
      Serial.println(audioVolume);
      return true;
      
    case CMD_PWM_UP:
      rangePWM += 25;
      if (rangePWM > 255) rangePWM = 255;
      Serial.print("PWM range increased to ");
      Serial.println(rangePWM);
      return true;
      
    case CMD_PWM_DOWN:
      rangePWM -= 25;
      if (rangePWM < 0) rangePWM = 0;
      Serial.print("PWM range decreased to ");
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
      Serial.print("Unknown command ");
      Serial.println(cmd);
      Serial.println("Type 'h' for available commands");
      return false;
  }
  
  return false;
}

/**
 * Processes string messages that start with ':'
 * @param msg Pointer to the message string
 * @return True if message processed successfully
 */
bool processMessage(char* msg) {
  char* content = msg;
  if (content[0] == ':') {
    content++;
  }
  
  if (strlen(content) == 0) {
    Serial.println("Empty message received");
    return false;
  }

  // Help command
  if (strcmp(content, "help") == 0) {
    Serial.println("Help command received via message");
    processCommand(CMD_HELP);
    return true;
  }
  // Report command
  else if (strcmp(content, "report") == 0) {
    Serial.println("Report command received via message");
    processCommand(CMD_REPORT);
    return true;
  }

  else if (strcmp(content, MSG_REQUEST_SEASHELL) == 0) {
    if (PLAYER_ID == 2) {
      // This player (seashell) should respond
      Serial.println("Report command for seashell received via message");
      delay(10);
      sendStatusToLeader();
      return true;
    } else if (PLAYER_ID == 1) {
      // This player (small) should not respond - temporarily disable Serial3
      Serial3.end();  // Disable the serial port
      delay(250);     // Give time for the other player to respond
      Serial3.begin(9600);  // Re-enable with same baud rate
      return true;
    }
  }

  else if (strcmp(content, MSG_REQUEST_SMALL) == 0) {
    if (PLAYER_ID == 1) {
      // This player (small) should respond
      Serial.println("Report command for small received via message");
      delay(10);
      sendStatusToLeader();
      return true;
    } else if (PLAYER_ID == 2) {
      // This player (seashell) should not respond - temporarily disable Serial3
      Serial3.end();  // Disable the serial port
      delay(250);     // Give time for the other player to respond
      Serial3.begin(9600);  // Re-enable with same baud rate
      return true;
    }
  }

  else if (strncmp(content, "STATUS|", 7) == 0) {
    // This is a status message from a follower
    Serial.println("Status received from follower:");
    
    // Parse the status message
    char* token = strtok(content + 7, "|"); // Skip "STATUS|" prefix
    
    if (token != NULL) {
      int followerId = atoi(token);
      Serial.print("Player ID: ");
      Serial.println(followerId);
      
      token = strtok(NULL, "|");
      if (token != NULL) {
        float followerTemp = atof(token);
        Serial.print("CPU Temperature: ");
        Serial.print(followerTemp);
        Serial.println(" °C");
        
        token = strtok(NULL, "|");
        if (token != NULL) {
          bool followerAwake = (atoi(token) == 1);
          Serial.print("System Awake: ");
          Serial.println(followerAwake ? "YES" : "NO");
          
          token = strtok(NULL, "|");
          if (token != NULL) {
            bool followerPlaying = (atoi(token) == 1);
            Serial.print("Playback Status: ");
            Serial.println(followerPlaying ? "PLAYING" : "STOPPED");
            
            token = strtok(NULL, "|");
            if (token != NULL) {
              uint32_t followerPosition = strtoul(token, NULL, 10);
              
              token = strtok(NULL, "|");
              if (token != NULL) {
                uint32_t followerLength = strtoul(token, NULL, 10);
                
                if (followerLength > 0) {
                  Serial.print("Playback Position: ");
                  Serial.print(formatTimeToMinutesSecondsMs(followerPosition));
                  Serial.print(" / ");
                  Serial.println(formatTimeToMinutesSecondsMs(followerLength));
                }
              }
            }
          }
        }
      }
    }
    
    return true;
  }

  // Wake up command
  else if (strcmp(content, "wakeup") == 0) {
    Serial.println("Wakeup command received via message");
    processCommand(CMD_WAKEUP);
    return true;
  }
  // Sleep command
  else if (strcmp(content, "sleep") == 0) {
    Serial.println("Sleep command received via message");
    processCommand(CMD_SLEEP);
    return true;
  }
  // Play command
  else if (strcmp(content, "play") == 0) {
    Serial.println("Play command received via message");
    processCommand(CMD_PLAY);
    return true;
  }
  // Stop command
  else if (strcmp(content, "stop") == 0) {
    Serial.println("Stop command received via message");
    processCommand(CMD_STOP);
    return true;
  }
  // Replay command
  else if (strcmp(content, "replay") == 0) {
    Serial.println("Replay command received via message");
    processCommand(CMD_REPLAY);
    return true;
  }
  // Volume up command
  else if (strcmp(content, "volup") == 0) {
    Serial.println("Volume up command received via message");
    processCommand(CMD_VOL_UP);
    return true;
  }
  // Volume down command
  else if (strcmp(content, "voldown") == 0) {
    Serial.println("Volume down command received via message");
    processCommand(CMD_VOL_DOWN);
    return true;
  }
  // PWM up command
  else if (strcmp(content, "pwmup") == 0) {
    Serial.println("PWM up command received via message");
    processCommand(CMD_PWM_UP);
    return true;
  }
  // PWM down command
  else if (strcmp(content, "pwmdown") == 0) {
    Serial.println("PWM down command received via message");
    processCommand(CMD_PWM_DOWN);
    return true;
  }
  // LED commands
  else if (strcmp(content, "led1") == 0) {
    Serial.println("LED 1 command received via message");
    processCommand(CMD_LED_1);
    return true;
  }
  else if (strcmp(content, "led2") == 0) {
    Serial.println("LED 2 command received via message");
    processCommand(CMD_LED_2);
    return true;
  }
  else if (strcmp(content, "led3") == 0) {
    Serial.println("LED 3 command received via message");
    processCommand(CMD_LED_3);
    return true;
  }
  else if (strcmp(content, "led4") == 0) {
    Serial.println("LED 4 command received via message");
    processCommand(CMD_LED_4);
    return true;
  }
  // Unknown command
  else {
    Serial.print("Unknown message: '");
    Serial.print(content);
    Serial.println("'");
    Serial.println("Type ':help' for available messages");
    return false;
  }
}

/**
 * Receives and processes messages from Serial3 (from the leader)
 */
void receiveSerialMessage() {
  // Clear the message buffer
  memset(messageBuffer, 0, MSG_BUFFER_SIZE);
  
  int index = 0;
  
  // Read the ':' character
  messageBuffer[index++] = (char)Serial3.read();
  
  // Read until end of message or buffer is full
  unsigned long startTime = millis();
  bool messageComplete = false;
  
  while (!messageComplete && index < MSG_BUFFER_SIZE - 1 && Serial3.available()) {
    char c = (char)Serial3.read();
    
    // Message is considered ended if a new line or ';' is read
    if (c == '\n' || c == '\r' || c == ';') {
      if (c == ';') {
        // Include the semicolon in the message
        messageBuffer[index++] = c;
      }
      messageComplete = true;
    } else {
      // Add character to buffer
      messageBuffer[index++] = c;
    }
    
    // Small delay to ensure all characters are received
    delay(1);
  }
  
  // Null-terminate the string
  messageBuffer[index] = '\0';
  
  // Print received message
  Serial.print("Received message ");
  Serial.println(messageBuffer);
  processMessage(messageBuffer);
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
      Serial.print("USB command received '");
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
          Serial.print("Unknown command '");
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

bool checkUsbMessages() {
  memset(messageBuffer, 0, MSG_BUFFER_SIZE);
  bool messageProcessed = false;

  // No data available
  if (!Serial.available()) {
    return messageProcessed;
  }

  // Peek for : as first character
  char firstChar = Serial.peek();
  if (firstChar != ':') {
    return messageProcessed;
  }

  // Read the ':' character
  Serial.read();
  
  // Initialize message buffer
  int index = 0;
  messageBuffer[index++] = ':';  // Store ':'

  // Read until end of message or buffer is full
  bool messageComplete = false;
  unsigned long startTime = millis();
  
  while (!messageComplete && index < MSG_BUFFER_SIZE - 1) {
    if (Serial.available()) {
      char c = Serial.read();
      
      // Message is considered ended if a new line or ';' is read
      if (c == '\n' || c == '\r' || c == ';') {
        if (c == ';') {
          // Include the semicolon in the message
          messageBuffer[index++] = c;
        }
        messageComplete = true;
      } else {
        // Add character to buffer
        messageBuffer[index++] = c;
      }
    } else if (millis() - startTime > 250) {
      // Timeout if no new data for 250ms
      messageComplete = true;
    }
  }

  // Null-terminate the string
  messageBuffer[index] = '\0';
  
  // Print received message
  Serial.print("Message received ");
  Serial.println(messageBuffer);
  processMessage(messageBuffer);
  
  // If this is the leader player and we have a valid message, forward it
  if (PLAYER_ID == 0 && index > 1) {
    // Use a single print statement to avoid formatting issues
    Serial.print("Message '");
    Serial.print(messageBuffer);
    Serial.println("' was sent on Serial3");
    
    // Actually send the message
    Serial3.print(messageBuffer);
  }
  
  messageProcessed = true;
  return messageProcessed;
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

#endif // MYSYSCTRL_H