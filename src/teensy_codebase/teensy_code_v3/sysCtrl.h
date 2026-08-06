/**
 * sysCtrl.h - System control for Suuret Muinaiset (V2 firmware)
 *
 * One leader (LONG / ID0, has RTC) drives two followers (SMALL ID1,
 * SEASHELL ID2) over a single half-duplex Serial3 link: leader TX -> both
 * follower RX, followers TX -> leader RX (multidrop back-channel).
 *
 * Design goals vs. the original:
 *   - No infinite while() traps. Failed init degrades and keeps retrying;
 *     the loop, serial link and watchdog stay alive.
 *   - Hardware watchdog auto-recovers from any hang.
 *   - Addressed + checksummed serial frames. Only the addressed unit
 *     replies, so the old Serial3.end()/begin() contention hack is gone.
 *     Malformed frames are dropped, never block, never flush good data.
 *   - RTC is never silently overwritten with compile time. It is only
 *     used as a last resort when the stored year is implausible. Time is
 *     set explicitly with ':settime ...' or ':synctime'.
 *   - Relay polarity is a single flag (RELAY_ACTIVE_LOW). Boot state is
 *     always de-energized. Switching is edge-driven with dwell.
 *   - Playback is leader-broadcast each loop, so the three units resync
 *     every cycle instead of drifting.
 *
 * NOTE on includes: this header defines function bodies and is meant to be
 * #included exactly once from the .ino, after the Audio/RTC objects are
 * declared there. Cross-references are resolved by the prototypes block.
 */

#ifndef SYSCTRL_H
#define SYSCTRL_H

#include <Arduino.h>
#include <RTClib.h>

// -------- objects defined in the .ino --------
extern AudioPlaySdWav        wavPlayer;
extern AudioAnalyzePeak      audioPeak;
extern AudioAnalyzeRMS       audioRMS;
extern AudioControlSGTL5000  sgtl5000;
extern RTC_DS3231            rtc;

// -------- pins / config defined in the .ino --------
extern const int SDCARD_CS_PIN, SDCARD_MOSI_PIN, SDCARD_SCK_PIN;
extern const int REL_1, REL_2;
extern const int PWM_PIN;
extern const int SMALL_PIN, SEASHELL_PIN, LONG_PIN;
extern const uint8_t VOL_CTRL_PIN;
extern const uint8_t LED_ARRAY[];

extern const bool  RELAY_ACTIVE_LOW;
extern const int   START_HOUR, END_HOUR;
extern const bool  PEAK_MODE;
extern const int   REL_SW_DELAY;     // ms dwell between relay switches
extern const int   LOOP_GAP_MS;      // gap before leader retriggers a round
extern const int   WDT_TIMEOUT_S;    // watchdog window

// -------- mutable state defined in the .ino --------
extern int   PLAYER_ID;              // 0 LONG, 1 SMALL, 2 SEASHELL
extern char  FILE_NAME[];
extern const char SM_STR[], SS_STR[], LO_STR[];
extern bool  systemAwake;
extern bool  playbackStatus;
extern int   trackIteration;
extern float audioVolume;
extern bool  knobCtrl;
extern int   rangePWM;
extern int   pwmFreq;                // ms between PWM updates

// -------- status codes shown on the LED array --------
enum : int {
  CODE_SLEEP    = 1,   // asleep, normal
  CODE_AWAKE    = 2,   // awake, idle
  CODE_SD_ERR   = 3,   // SD not available
  CODE_CODEC_ERR= 4,   // SGTL5000 not found
  CODE_RTC_ERR  = 5,   // RTC not found
  CODE_LINK_ERR = 6,   // lost serial link to leader (followers)
  CODE_REBOOT   = 7,   // rebooting shortly
  CODE_PLAYING  = 8    // audio playing
};

// -------- forward prototypes (resolve mutual calls) --------
void displayBinaryCode(int);                 // from ledzCtrl.h
void setRelay(int pin, bool on);
void applyAwakeState(bool target);
void startupSequence();
void shutDownSequence();
void playRound();
void stopAudio();
void writeOutPWM(uint8_t pin);
void volumeControl();
void setVolume(float v);
void sendFrame(char addr, char cmd, const char *arg);
void serialPoll();
void dispatchCommand(char cmd, const char *arg);
void usbPoll();
void setupPlayerID();
void setupRTC();
void clockMe();
void systemReport();
void scheduledReboot();
void feedWatchdog();                         // defined in the .ino (knows the WDT object)

// -------- serial protocol --------
// Frame on the wire:  #<addr><cmd><arg>*<XX>\n
//   addr : '0' all followers, '1' small, '2' seashell, 'L' leader (replies)
//   cmd  : single char (see below)
//   arg  : optional short ASCII payload
//   XX   : 2-hex XOR checksum of every char between '#' and '*'
static const char ADDR_ALL='0', ADDR_SMALL='1', ADDR_SEASHELL='2', ADDR_LEADER='L';

// command chars (used on both Serial3 frames and single-key USB control)
static const char CMD_PLAY='P', CMD_STOP='X', CMD_WAKE='W', CMD_SLEEP='S';
static const char CMD_HEART='H', CMD_REPORT='R', CMD_STATUS='s', CMD_REBOOT='B';
static const char CMD_VOL='V', CMD_VOLUP='+', CMD_VOLDN='-';
static const char CMD_PWMUP='>', CMD_PWMDN='<', CMD_KNOB='K', CMD_HELP='?';

static inline char myAddr() {
  return PLAYER_ID == 1 ? ADDR_SMALL : PLAYER_ID == 2 ? ADDR_SEASHELL : ADDR_LEADER;
}
static inline uint8_t xorChk(const char *s, int n) {
  uint8_t c = 0; for (int i = 0; i < n; i++) c ^= (uint8_t)s[i]; return c;
}

void sendFrame(char addr, char cmd, const char *arg = "") {
  char body[48];
  int n = snprintf(body, sizeof(body), "%c%c%s", addr, cmd, arg);
  if (n < 0 || n >= (int)sizeof(body)) return;
  uint8_t c = xorChk(body, n);
  Serial3.write('#');
  Serial3.write((const uint8_t *)body, n);
  Serial3.write('*');
  char hex[3]; snprintf(hex, sizeof(hex), "%02X", c);
  Serial3.write((const uint8_t *)hex, 2);
  Serial3.write('\n');
}

// ===================== player identity =====================
void setupPlayerID() {
  pinMode(SMALL_PIN, INPUT);
  pinMode(SEASHELL_PIN, INPUT);
  pinMode(LONG_PIN, INPUT);

  if (digitalRead(LONG_PIN) == HIGH)          PLAYER_ID = 0;
  else if (digitalRead(SMALL_PIN) == HIGH)    PLAYER_ID = 1;
  else if (digitalRead(SEASHELL_PIN) == HIGH) PLAYER_ID = 2;
  else PLAYER_ID = 0; // default to leader if no ID strap is read

  if      (PLAYER_ID == 0) strcpy(FILE_NAME, LO_STR);
  else if (PLAYER_ID == 1) strcpy(FILE_NAME, SM_STR);
  else                     strcpy(FILE_NAME, SS_STR);

  Serial.print("Player ID "); Serial.print(PLAYER_ID);
  Serial.print(" file "); Serial.println(FILE_NAME);
}

// ===================== RTC =====================
// Non-fatal: on failure we flag CODE_RTC_ERR and return. The unit keeps
// running on the leader's broadcast state if the clock is unavailable.
void setupRTC() {
  if (!rtc.begin()) {
    Serial.println("RTC not found");
    displayBinaryCode(CODE_RTC_ERR);
    return;
  }
  if (rtc.lostPower()) {
    DateTime n = rtc.now();
    Serial.println("RTC reports lost power (check the coin cell).");
    // Only fall back to compile time if stored time is clearly invalid.
    if (n.year() < 2024) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("RTC year < 2024, set to compile time as a fallback.");
    } else {
      Serial.println("Stored time looks plausible, keeping it.");
    }
  }
}

void clockMe() {
  DateTime t = rtc.now();
  char buf[40];
  snprintf(buf, sizeof(buf), "%04u/%02u/%02u %02u:%02u:%02u",
           t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
  Serial.println(buf);
}

// ===================== relays / power sequencing =====================
void setRelay(int pin, bool on) {
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

void startupSequence() {
  if (systemAwake) return;
  setRelay(REL_1, true);  Serial.println("amp ON");
  feedWatchdog(); delay(REL_SW_DELAY);
  setRelay(REL_2, true);  Serial.println("speaker ON");
  feedWatchdog(); delay(REL_SW_DELAY);
  systemAwake = true;
  trackIteration = 0;
}

void shutDownSequence() {
  if (!systemAwake) return;
  stopAudio();
  setRelay(REL_2, false); Serial.println("speaker OFF");
  feedWatchdog(); delay(REL_SW_DELAY);
  setRelay(REL_1, false); Serial.println("amp OFF");
  feedWatchdog(); delay(REL_SW_DELAY);
  systemAwake = false;
}

void applyAwakeState(bool target) {
  if (target == systemAwake) return;
  if (target) startupSequence(); else shutDownSequence();
}

// ===================== audio =====================
void setVolume(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  audioVolume = v;
  sgtl5000.volume(audioVolume);
  Serial.print("Volume "); Serial.println(audioVolume);
}

void playRound() {
  if (!systemAwake) return;
  wavPlayer.stop();
  if (!wavPlayer.play(FILE_NAME)) {
    Serial.print("play() failed for "); Serial.println(FILE_NAME);
    return;
  }
  trackIteration++;
  playbackStatus = true;
  Serial.print("Playing "); Serial.print(FILE_NAME);
  Serial.print("  iter "); Serial.println(trackIteration);
}

void stopAudio() {
  wavPlayer.stop();
  playbackStatus = false;
  analogWrite(PWM_PIN, 0);
}

void writeOutPWM(uint8_t pin) {
  static elapsedMillis pwmTimer;
  if (pwmTimer < (unsigned)pwmFreq) return;
  pwmTimer = 0;
  float level = 0.0f;
  if (PEAK_MODE) { if (audioPeak.available()) level = audioPeak.read(); else return; }
  else           { if (audioRMS.available())  level = audioRMS.read();  else return; }
  int v = (int)(level * rangePWM);
  if (v < 0) v = 0; if (v > 255) v = 255;
  analogWrite(pin, v);
}

void volumeControl() {
  static elapsedMillis t;
  if (t < 50) return;
  t = 0;
  float raw = analogRead(VOL_CTRL_PIN) / 1024.0f;
  float q = roundf(raw * 10.0f) / 10.0f;
  if (q != audioVolume) {
    setVolume(q);
    if (PLAYER_ID == 0) {
      char arg[8]; snprintf(arg, sizeof(arg), "%.1f", audioVolume);
      sendFrame(ADDR_ALL, CMD_VOL, arg);   // keep followers in step
    }
  }
}

// ===================== command dispatch =====================
// Applies a command locally. 'arg' is the optional payload (e.g. volume).
void dispatchCommand(char cmd, const char *arg) {
  switch (cmd) {
    case CMD_PLAY:   playRound(); break;
    case CMD_STOP:   stopAudio(); break;
    case CMD_WAKE:   applyAwakeState(true); break;
    case CMD_SLEEP:  applyAwakeState(false); break;
    case CMD_REPORT: systemReport(); break;
    case CMD_REBOOT: scheduledReboot(); break;
    case CMD_VOL:    if (arg && *arg) setVolume(atof(arg)); break;
    case CMD_VOLUP:  setVolume(audioVolume + 0.1f); break;
    case CMD_VOLDN:  setVolume(audioVolume - 0.1f); break;
    case CMD_PWMUP:  rangePWM = min(255, rangePWM + 25);
                     Serial.print("PWM range "); Serial.println(rangePWM); break;
    case CMD_PWMDN:  rangePWM = max(0, rangePWM - 25);
                     Serial.print("PWM range "); Serial.println(rangePWM); break;
    case CMD_KNOB:   knobCtrl = !knobCtrl;
                     Serial.print("Knob ctrl "); Serial.println(knobCtrl ? "ON" : "OFF"); break;
    case CMD_HEART:  // followers sync awake-state from the heartbeat payload
                     if (PLAYER_ID != 0 && arg && *arg)
                       applyAwakeState(arg[0] == '1');
                     break;
    case CMD_HELP:
      Serial.println(F("\nKeys: P play  X stop  W wake  S sleep  R report"));
      Serial.println(F("      + / - volume   > / < PWM range   K knob   B reboot"));
      Serial.println(F("      :settime YYYY MM DD HH MM SS   :synctime (compile time)"));
      break;
    default: break;
  }
}

// ===================== incoming Serial3 frames =====================
// Builds one frame at a time without blocking and without discarding
// other queued bytes. Bad checksums / overflows are dropped silently.
static void handleFrame(char *f, int n) {
  char *star = strchr(f, '*');
  if (!star || n < 3) return;
  int blen = star - f;
  uint8_t want = (uint8_t)strtol(star + 1, NULL, 16);
  if (xorChk(f, blen) != want) return;           // corrupted, ignore

  char addr = f[0], cmd = f[1];
  char arg[40]; int al = blen - 2; if (al < 0) al = 0;
  if (al > (int)sizeof(arg) - 1) al = sizeof(arg) - 1;
  memcpy(arg, f + 2, al); arg[al] = 0;

  if (PLAYER_ID == 0) {
    // leader only listens to replies addressed to it
    if (addr == ADDR_LEADER && cmd == CMD_STATUS) {
      Serial.print("Follower status: "); Serial.println(arg);
    }
    return;
  }
  // followers act on broadcast or their own address
  bool forMe = (addr == ADDR_ALL) || (addr == myAddr());
  if (!forMe) return;

  if (cmd == CMD_REPORT) {                        // polled -> reply once
    char s[40];
    snprintf(s, sizeof(s), "%d,%d,%d,%.1f",
             PLAYER_ID, systemAwake ? 1 : 0, playbackStatus ? 1 : 0,
             tempmonGetTemp());
    sendFrame(ADDR_LEADER, CMD_STATUS, s);
  } else {
    dispatchCommand(cmd, arg);
  }
}

void serialPoll() {
  static char buf[64];
  static int idx = 0;
  static bool inFrame = false;
  while (Serial3.available()) {
    char ch = (char)Serial3.read();
    if (ch == '#') { inFrame = true; idx = 0; continue; }
    if (!inFrame) continue;
    if (ch == '\n') { buf[idx] = 0; handleFrame(buf, idx); inFrame = false; idx = 0; continue; }
    if (idx < (int)sizeof(buf) - 1) buf[idx++] = ch;
    else { inFrame = false; idx = 0; }            // overflow -> drop frame
  }
}

// ===================== USB console =====================
// Single keys map straight to commands. Lines beginning ':' are parsed
// for time setting. On the leader, keys are also broadcast to followers.
void usbPoll() {
  if (!Serial.available()) return;

  if (Serial.peek() == ':') {
    char line[48]; int i = 0;
    Serial.read();                                // consume ':'
    unsigned long t0 = millis();
    while (i < (int)sizeof(line) - 1) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        line[i++] = c; t0 = millis();
      } else if (millis() - t0 > 200) break;
    }
    line[i] = 0;

    if (strncmp(line, "settime", 7) == 0) {
      int Y, Mo, D, H, Mi, S;
      if (sscanf(line + 7, "%d %d %d %d %d %d", &Y, &Mo, &D, &H, &Mi, &S) == 6) {
        rtc.adjust(DateTime(Y, Mo, D, H, Mi, S));
        Serial.print("RTC set to "); clockMe();
      } else Serial.println("Usage: :settime YYYY MM DD HH MM SS");
    } else if (strncmp(line, "synctime", 8) == 0) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.print("RTC set to compile time "); clockMe();
    } else if (strncmp(line, "time", 4) == 0) {
      clockMe();
    } else {
      Serial.println("Unknown : command");
    }
    return;
  }

  char k = Serial.read();
  if (k <= 32) return;                            // ignore whitespace
  if (PLAYER_ID == 0 &&
      (k == CMD_PLAY || k == CMD_STOP || k == CMD_WAKE || k == CMD_SLEEP ||
       k == CMD_VOLUP || k == CMD_VOLDN || k == CMD_PWMUP || k == CMD_PWMDN ||
       k == CMD_REBOOT)) {
    sendFrame(ADDR_ALL, k, "");                   // relay to followers
  }
  dispatchCommand(k, "");
}

// ===================== report =====================
void systemReport() {
  Serial.println(F("\n----- SYSTEM REPORT -----"));
  Serial.print("Player ID "); Serial.println(PLAYER_ID);
  Serial.print("File "); Serial.println(FILE_NAME);
  if (PLAYER_ID == 0) { Serial.print("RTC "); clockMe(); }
  Serial.print("Awake "); Serial.println(systemAwake ? "YES" : "NO");
  Serial.print("Playing "); Serial.println(playbackStatus ? "YES" : "NO");
  Serial.print("Volume "); Serial.println(audioVolume);
  Serial.print("PWM range "); Serial.println(rangePWM);
  Serial.print("Track iter "); Serial.println(trackIteration);
  Serial.print("CPU temp "); Serial.print(tempmonGetTemp()); Serial.println(" C");
  Serial.print("Audio mem max "); Serial.println(AudioMemoryUsageMax());
  Serial.println(F("----- END REPORT -----\n"));
}

// ===================== reboot =====================
void scheduledReboot() {
  Serial.println("Rebooting shortly.");
  if (PLAYER_ID == 0) sendFrame(ADDR_ALL, CMD_REBOOT, "");
  if (systemAwake) shutDownSequence();
  displayBinaryCode(CODE_REBOOT);
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) feedWatchdog();    // brief, watchdog kept fed
  SCB_AIRCR = 0x05FA0004;                          // system reset
}

#endif // SYSCTRL_H
