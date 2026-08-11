/**
 * config.h - compile-time configuration for Suuret Muinaiset (V2)
 *
 * All pins, timings and protocol constants live here. Nothing in this file
 * allocates storage that varies at runtime; values are constexpr so they
 * cost nothing and can be used in any translation unit.
 */
#ifndef SM_CONFIG_H
#define SM_CONFIG_H

#include <Arduino.h>

// Set to 0 to build without the hardware watchdog (e.g. if Watchdog_t4 is
// not installed). With 1, any hang self-recovers after WDT_TIMEOUT_S.
//
// !!! TEMPORARILY 0 FOR THE AUDIO-DISTORTION LOGGING SESSION !!!
// A hang will NOT self-recover while this is 0. Set it back to 1 before the
// units are left running unattended.
#define USE_WATCHDOG 0

namespace cfg {
  // ---- hardware ID strap pins (HIGH selects the role) ----
  constexpr uint8_t PIN_LONG     = 32;   // leader
  constexpr uint8_t PIN_SMALL    = 30;
  constexpr uint8_t PIN_SEASHELL = 28;

  // ---- status LED array (MSB first: LED_PINS[0] = bit3) ----
  constexpr uint8_t LED_PINS[4]  = { 2, 3, 4, 5 };

  // ---- relays ----
  constexpr uint8_t  REL_AMP        = 17;   // amp / 36V PSU
  constexpr uint8_t  REL_SPK        = 16;   // speaker
  constexpr bool     RELAY_ACTIVE_LOW = false; // FLIP if relays energize at
                                               // boot or behave inverted
  constexpr uint16_t RELAY_DWELL_MS = 500;  // settle time between switches

  // ---- audio-to-light PWM ----
  constexpr uint8_t  PWM_PIN        = 6;
  constexpr uint8_t  PWM_UPDATE_MS  = 25;   // ~40 Hz refresh
  constexpr bool     PEAK_MODE      = true; // follow peak (true) or RMS

  // ---- SD card ----
  constexpr uint8_t SD_CS   = 10;
  constexpr uint8_t SD_MOSI = 11;
  constexpr uint8_t SD_SCK  = 13;

  // ---- volume ----
  constexpr uint8_t VOL_POT        = A8;
  constexpr float   DEFAULT_VOLUME = 0.6f;  // headroom; raise carefully

  // ---- daily schedule (leader, from RTC) ----
  constexpr uint8_t START_HOUR = 8;
  constexpr uint8_t END_HOUR   = 20;

  // ---- timing ----
  constexpr uint16_t LOOP_GAP_MS   = 500;   // gap before next synchronized round
  constexpr uint8_t  WDT_TIMEOUT_S = 15;

  // ---- audio files (8.3 names on the SD card) ----
  constexpr char FILE_LONG[]     = "LONG.WAV";
  constexpr char FILE_SMALL[]    = "SMALL.WAV";
  constexpr char FILE_SEASHELL[] = "SEASHELL.WAV";
}

// Player role; numeric values match the legacy PLAYER_ID.
enum class Role : uint8_t { LEADER = 0, SMALL = 1, SEASHELL = 2 };

// LED status codes (0-15).
namespace status {
  constexpr int SLEEP = 1, AWAKE = 2, SD_ERR = 3, CODEC_ERR = 4,
                RTC_ERR = 5, LINK_ERR = 6, REBOOT = 7, PLAYING = 8;
}

// Serial3 protocol.  Frame:  #<addr><cmd><arg>*<XX>\n
namespace proto {
  constexpr char ADDR_ALL = '0', ADDR_SMALL = '1',
                 ADDR_SEASHELL = '2', ADDR_LEADER = 'L';
  constexpr char CMD_PLAY = 'P', CMD_STOP = 'X', CMD_WAKE = 'W', CMD_SLEEP = 'S';
  constexpr char CMD_HEART = 'H', CMD_REPORT = 'R', CMD_STATUS = 's', CMD_REBOOT = 'B';
  constexpr char CMD_VOL = 'V', CMD_VOLUP = '+', CMD_VOLDN = '-';
  constexpr char CMD_PWMUP = '>', CMD_PWMDN = '<', CMD_KNOB = 'K', CMD_HELP = '?';
}

#endif // SM_CONFIG_H
