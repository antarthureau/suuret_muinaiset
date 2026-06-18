/**
 * mySysCtrl.h - System Control Library
 *
 * Functions for system state, timing, player identification, audio playback,
 * serial communication and system reboot and reports.
 */

#ifndef MYSYSCTRL_H
#define MYSYSCTRL_H

#include <Arduino.h>
#include <RTClib.h>
#include <Audio.h>

// External references to variables defined in the main program
extern int PLAYER_ID;            // Current player ID (0=LONG, 1=SMALL, 2=SEASHELL)
extern char FILE_NAME[];         // Current audio file name
extern bool systemAwake;         // System active state
extern bool playbackStatus;      // Audio playback state
extern int trackIteration;       // Track play count for current day
extern bool messageIncoming;
extern const int MSG_BUFFER_SIZE;
extern char messageBuffer[];
extern bool knobCtrl;

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

// Command definitions
#define CMD_LED_1 '1'  // LED 1 control
#define CMD_LED_2 '2'  // LED 2 control
#define CMD_LED_3 '3'  // LED 3 control
#define CMD_LED_4 '4'  // LED 4 control
#define CMD_HELP 'H'   // Display help
#define CMD_WAKEUP 'W' // Wake up system
#define CMD_PLAY 'P'   // Play audio
#define CMD_SLEEP 'S'  // Sleep system
#define CMD_STOP '!'   // Stop audio
#define CMD_REPLAY 'Z' // Reset and replay audio
#define CMD_REPORT 'R' // Generate system report
#define CMD_VOL_UP '+'  // Increase volume
#define CMD_VOL_DOWN '-' // Decrease volume
#define CMD_PWM_UP '>'  // Increase PWM range
#define CMD_PWM_DOWN '<' // Decrease PWM range
#define CMD_REBOOT 'B'  // Reboot system
#define CMD_KNOB_CTRL 'K' //toggles extern analog mode

// Function declarations
void setupPlayerID();
void clockMe();
String formatTimeToMinutesSecondsMs(unsigned long ms);
void systemReport(int player);
void startupSequence();
void shutDownSequence();
void playAudio();
void sendSerialCommand(char command);
void sendSerialMessage(char* message);
void volumeControl();
void sendStatusToLeader();
void scheduledReboot();
bool processCommand(char cmd);
bool processMessage(char* msg);
void receiveSerialMessage();
bool checkUsbCommands();
bool checkUsbMessages();
void statusUpdates();

#endif // MYSYSCTRL_H
