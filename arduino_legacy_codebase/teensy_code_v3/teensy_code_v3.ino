/*
  Suuret Muinaiset - V3 firmware (Teensy 4.0)
  Sound-to-light installation, Turku.

  3 identical units, role chosen by hardware ID strap pins:
    LONG     ID0  leader,  has RTC, plays LONG.WAV
    SMALL    ID1  follower,          plays SMALL.WAV
    SEASHELL ID2  follower,          plays SEASHELL.WAV

  The leader keeps time, drives wake/sleep, and broadcasts a synchronized
  PLAY each loop so the three resync every cycle. Communication is over a
  single Serial3 link using short addressed, checksummed frames (see
  sysCtrl.h). All blocking init traps were removed and a hardware watchdog
  recovers from any hang.
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <elapsedMillis.h>
#include <RTClib.h>
#include <Watchdog_t4.h>     // bundled with Teensyduino; set USE_WATCHDOG 0 to disable
#include "ledzCtrl.h"
#include "sysCtrl.h"

#define USE_WATCHDOG 1

// -------- audio objects --------
AudioPlaySdWav        wavPlayer;
AudioAnalyzePeak      audioPeak;
AudioAnalyzeRMS       audioRMS;
AudioOutputI2S        audioOutput;
AudioControlSGTL5000  sgtl5000;
AudioConnection       pc1(wavPlayer, 0, audioOutput, 0);
AudioConnection       pc2(wavPlayer, 0, audioOutput, 1);   // mono to both channels
AudioConnection       pc3(wavPlayer, 0, audioPeak, 0);
AudioConnection       pc4(wavPlayer, 0, audioRMS, 0);
RTC_DS3231            rtc;

#if USE_WATCHDOG
WDT_T4<WDOG1> wdt;
#endif
void feedWatchdog() {
#if USE_WATCHDOG
  wdt.feed();
#endif
}

// -------- pins (match PINS.txt) --------
const int SDCARD_CS_PIN = 10, SDCARD_MOSI_PIN = 11, SDCARD_SCK_PIN = 13;
const int REL_1 = 17, REL_2 = 16;
const int LED_1 = 2, LED_2 = 3, LED_3 = 4, LED_4 = 5;
const int PWM_PIN = 6;
const int SMALL_PIN = 30, SEASHELL_PIN = 28, LONG_PIN = 32;
const uint8_t VOL_CTRL_PIN = A8;
const uint8_t LED_ARRAY[4] = { LED_1, LED_2, LED_3, LED_4 };

/* ============================================================
 *  THINGS YOU CAN CHANGE
 * ============================================================ */
float      audioVolume    = 0.6;    // 0.0-1.0; lower gives the amp headroom
bool       knobCtrl       = false;  // true = read A8 pot for volume (leader)
const int  START_HOUR     = 8;      // daily wake hour
const int  END_HOUR       = 20;     // daily sleep hour
const bool RELAY_ACTIVE_LOW= false; // FLIP to true if relays energize at boot
                                    // or behave inverted (opto-isolated boards)
const int  LOOP_GAP_MS    = 500;    // gap before leader starts the next round
const int  REL_SW_DELAY   = 500;    // dwell between relay switches
const int  WDT_TIMEOUT_S  = 15;     // watchdog window (s)
const bool PEAK_MODE      = true;   // PWM follows peak (true) or RMS (false)
/* ============================================================ */

int   rangePWM     = 255;
int   pwmFreq      = 25;            // ms between PWM updates (~40 Hz)
int   trackIteration = 0;
bool  systemAwake  = false;
bool  playbackStatus = false;

const char SM_STR[13] = "SMALL.WAV";
const char SS_STR[13] = "SEASHELL.WAV";
const char LO_STR[13] = "LONG.WAV";
char  FILE_NAME[13];
int   PLAYER_ID;

// init flags so we can keep retrying instead of hanging
static bool sdReady = false, codecReady = false;

// =====================================================================
void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);

  if (CrashReport) { Serial.print("Crash: "); Serial.println(CrashReport); }

  setupPlayerID();

  for (int i = 0; i < 4; i++) { pinMode(LED_ARRAY[i], OUTPUT); digitalWrite(LED_ARRAY[i], LOW); }

  // relays: de-energized at boot regardless of polarity
  pinMode(REL_1, OUTPUT); setRelay(REL_1, false);
  pinMode(REL_2, OUTPUT); setRelay(REL_2, false);

  pinMode(PWM_PIN, OUTPUT); analogWrite(PWM_PIN, 0);
  analogWriteResolution(8);

  AudioMemory(60);                  // generous; RAM is cheap on a Teensy 4.0.
  // Dropouts come from SD read latency and from blocking work in the loop,
  // not from this number, so the real fixes are: no delay() in the audio
  // path, and the watchdog/non-blocking loop below. Buffer just adds slack.

  codecReady = sgtl5000.enable();
  if (codecReady) sgtl5000.volume(audioVolume);
  else { Serial.println("Codec enable failed"); displayBinaryCode(CODE_CODEC_ERR); }

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  sdReady = SD.begin(SDCARD_CS_PIN);
  if (!sdReady) { Serial.println("SD not ready (will retry)"); displayBinaryCode(CODE_SD_ERR); }

  if (PLAYER_ID == 0) setupRTC();

#if USE_WATCHDOG
  WDT_timings_t cfg;
  cfg.trigger = WDT_TIMEOUT_S;      // seconds
  wdt.begin(cfg);
#endif

  Serial.println("Setup complete.");
}

// ---- timers ----
elapsedMillis statusTimer;          // leader RTC check
elapsedMillis heartTimer;           // leader heartbeat
elapsedMillis pollTimer;            // leader status poll
elapsedMillis linkTimer;            // follower link-loss
elapsedMillis gapTimer;             // round spacing
elapsedMillis retryTimer;           // SD/codec recovery

// =====================================================================
void loop() {
  feedWatchdog();

  // recover failed init without ever blocking the loop
  if ((!sdReady || !codecReady) && retryTimer >= 3000) {
    retryTimer = 0;
    if (!codecReady) { codecReady = sgtl5000.enable(); if (codecReady) sgtl5000.volume(audioVolume); }
    if (!sdReady)    { sdReady = SD.begin(SDCARD_CS_PIN); }
  }

  usbPoll();
  serialPoll();

  if (PLAYER_ID == 0) leaderTask();
  else                followerTask();
}

// =====================================================================
void leaderTask() {
  // 1) wake/sleep from the clock, every 30 s
  if (statusTimer >= 30000) {
    statusTimer = 0;
    DateTime now = rtc.now();
    bool shouldWake = (now.hour() >= START_HOUR && now.hour() < END_HOUR);
    if (shouldWake != systemAwake) {
      sendFrame(ADDR_ALL, shouldWake ? CMD_WAKE : CMD_SLEEP, "");
      applyAwakeState(shouldWake);
    }
  }

  // 2) heartbeat carries awake-state so late-booting followers self-sync
  if (heartTimer >= 5000) {
    heartTimer = 0;
    sendFrame(ADDR_ALL, CMD_HEART, systemAwake ? "1" : "0");
  }

  // 3) drive a synchronized round
  if (systemAwake) {
    if (!wavPlayer.isPlaying()) {
      if (gapTimer >= (unsigned)LOOP_GAP_MS) {
        sendFrame(ADDR_ALL, CMD_PLAY, "");
        playRound();
        gapTimer = 0;
      }
      displayBinaryCode(CODE_AWAKE);
      analogWrite(PWM_PIN, 0);
    } else {
      writeOutPWM(PWM_PIN);
      if (knobCtrl) volumeControl();
      displayBinaryCode(CODE_PLAYING);
      gapTimer = 0;
    }
  } else {
    if (wavPlayer.isPlaying()) stopAudio();
    displayBinaryCode(CODE_SLEEP);
    analogWrite(PWM_PIN, 0);
  }

  // 4) poll followers for status, staggered so only one replies at a time
  if (pollTimer >= 20000) {
    pollTimer = 0;
    sendFrame(ADDR_SMALL, CMD_REPORT, "");
    feedWatchdog(); delay(150);
    sendFrame(ADDR_SEASHELL, CMD_REPORT, "");
  }
}

// =====================================================================
void followerTask() {
  if (Serial3.available()) linkTimer = 0;         // any traffic = link alive

  if (systemAwake) {
    if (wavPlayer.isPlaying()) {
      writeOutPWM(PWM_PIN);
      displayBinaryCode(CODE_PLAYING);
    } else {
      displayBinaryCode(CODE_AWAKE);              // waiting for next PLAY
      analogWrite(PWM_PIN, 0);
    }
  } else {
    if (wavPlayer.isPlaying()) stopAudio();
    // show link error if we've heard nothing from the leader for 90 s
    displayBinaryCode(linkTimer > 90000 ? CODE_LINK_ERR : CODE_SLEEP);
    analogWrite(PWM_PIN, 0);
  }
}
