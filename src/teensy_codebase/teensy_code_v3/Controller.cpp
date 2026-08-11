#include "Controller.h"

// ===================== role =====================
Role Controller::detectRole() {
  pinMode(cfg::PIN_LONG, INPUT);
  pinMode(cfg::PIN_SMALL, INPUT);
  pinMode(cfg::PIN_SEASHELL, INPUT);
  if (digitalRead(cfg::PIN_LONG) == HIGH)     return Role::LEADER;
  if (digitalRead(cfg::PIN_SMALL) == HIGH)    return Role::SMALL;
  if (digitalRead(cfg::PIN_SEASHELL) == HIGH) return Role::SEASHELL;
  return Role::LEADER;  // default to leader if no strap is read
}

const char *Controller::fileForRole() const {
  switch (role_) {
    case Role::SMALL:    return cfg::FILE_SMALL;
    case Role::SEASHELL: return cfg::FILE_SEASHELL;
    default:             return cfg::FILE_LONG;
  }
}

char Controller::myAddr() const {
  switch (role_) {
    case Role::SMALL:    return proto::ADDR_SMALL;
    case Role::SEASHELL: return proto::ADDR_SEASHELL;
    default:             return proto::ADDR_LEADER;
  }
}

// ===================== watchdog =====================
void Controller::feedWdt() {
#if USE_WATCHDOG
  wdt_.feed();
#endif
}

// ===================== setup =====================
void Controller::setup() {
  Serial.begin(9600);
  link_.begin(9600);

  if (CrashReport) { Serial.print("Crash: "); Serial.println(CrashReport); }

  role_ = detectRole();
  file_ = fileForRole();
  Serial.print("Role "); Serial.print((int)role_);
  Serial.print(" file "); Serial.println(file_);

  leds_.begin();
  relays_.begin();
  pinMode(cfg::PWM_PIN, OUTPUT);
  analogWriteResolution(8);
  analogWrite(cfg::PWM_PIN, 0);

  audio_.begin(cfg::DEFAULT_VOLUME);
  if (!audio_.codecReady()) { Serial.println("codec fail"); leds_.show(status::CODEC_ERR); }
  if (!audio_.sdReady())    { Serial.println("SD fail");    leds_.show(status::SD_ERR); }

  if (role_ == Role::LEADER) {
    if (!rtc_.begin()) { Serial.println("RTC fail"); leds_.show(status::RTC_ERR); }
  }

#if USE_WATCHDOG
  WDT_timings_t t;
  t.trigger = cfg::WDT_TIMEOUT_S;
  wdt_.begin(t);
#endif

  Serial.println("Setup complete.");
}

// ===================== loop =====================
void Controller::loop() {
  feedWdt();

  // recover failed init without ever blocking
  if ((!audio_.codecReady() || !audio_.sdReady()) && retryTimer_ >= 3000) {
    retryTimer_ = 0;
    audio_.retryInit();
  }

  usbPoll();

  Frame f;
  while (link_.poll(f)) handleFrame(f);

  playing_ = audio_.isPlaying();

  if (role_ == Role::LEADER) leaderTask();
  else                       followerTask();
}

// ===================== behaviour =====================
void Controller::applyAwake(bool target) {
  if (target == awake_) return;
  feedWdt();
  if (target) relays_.wake(); else relays_.sleep();
  awake_ = target;
  if (target) trackIter_ = 0;
  if (!target) audio_.stop();
}

void Controller::playRound() {
  if (!awake_) return;
  if (audio_.play(file_)) {
    trackIter_++;
    playing_ = true;
    Serial.print("Playing "); Serial.print(file_);
    Serial.print(" iter "); Serial.println(trackIter_);
  } else {
    Serial.print("play() failed: "); Serial.println(file_);
  }
}

void Controller::writePwm() {
  if (pwmTimer_ < cfg::PWM_UPDATE_MS) return;
  pwmTimer_ = 0;
  float lv = audio_.level(cfg::PEAK_MODE);
  if (lv < 0.0f) return;                 // no new sample
  int v = (int)(lv * rangePwm_);
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  analogWrite(cfg::PWM_PIN, v);
}

void Controller::volumeFromKnob() {
  static elapsedMillis t;
  if (t < 50) return;
  t = 0;
  float raw = analogRead(cfg::VOL_POT) / 1024.0f;
  float q = roundf(raw * 10.0f) / 10.0f;
  if (q != audio_.volume()) {
    audio_.setVolume(q);
    Serial.print("Volume "); Serial.println(audio_.volume());
    char arg[8]; snprintf(arg, sizeof(arg), "%.1f", audio_.volume());
    link_.send(proto::ADDR_ALL, proto::CMD_VOL, arg);
  }
}

// ===================== command dispatch =====================
void Controller::dispatch(char cmd, const char *arg) {
  switch (cmd) {
    case proto::CMD_PLAY:   playRound(); break;
    case proto::CMD_STOP:   audio_.stop(); playing_ = false; analogWrite(cfg::PWM_PIN, 0); break;
    case proto::CMD_WAKE:   applyAwake(true);  break;
    case proto::CMD_SLEEP:  applyAwake(false); break;
    case proto::CMD_REPORT: report(); break;
    case proto::CMD_REBOOT: reboot(); break;
    case proto::CMD_VOL:    if (arg && *arg) { audio_.setVolume(atof(arg));
                              Serial.print("Volume "); Serial.println(audio_.volume()); } break;
    case proto::CMD_VOLUP:  audio_.setVolume(audio_.volume() + 0.1f);
                            Serial.print("Volume "); Serial.println(audio_.volume()); break;
    case proto::CMD_VOLDN:  audio_.setVolume(audio_.volume() - 0.1f);
                            Serial.print("Volume "); Serial.println(audio_.volume()); break;
    case proto::CMD_PWMUP:  rangePwm_ += 25; if (rangePwm_ > 255) rangePwm_ = 255;
                            Serial.print("PWM "); Serial.println(rangePwm_); break;
    case proto::CMD_PWMDN:  rangePwm_ -= 25; if (rangePwm_ < 0) rangePwm_ = 0;
                            Serial.print("PWM "); Serial.println(rangePwm_); break;
    case proto::CMD_KNOB:   knob_ = !knob_;
                            Serial.print("Knob "); Serial.println(knob_ ? "ON" : "OFF"); break;
    case proto::CMD_HEART:  if (role_ != Role::LEADER && arg && *arg) {
                              linkTimer_ = 0;
                              applyAwake(arg[0] == '1');
                            } break;
    case proto::CMD_HELP:
      Serial.println(F("\nKeys: P play  X stop  W wake  S sleep  R report  B reboot"));
      Serial.println(F("      + - volume   > < PWM   K knob"));
      Serial.println(F("      :settime YYYY MM DD HH MM SS   :synctime   :time"));
      break;
    default: break;
  }
}

// ===================== incoming frames =====================
void Controller::handleFrame(const Frame &f) {
  if (role_ == Role::LEADER) {
    if (f.addr == proto::ADDR_LEADER && f.cmd == proto::CMD_STATUS) {
      Serial.print("Follower status: "); Serial.println(f.arg);
    }
    return;
  }
  bool forMe = (f.addr == proto::ADDR_ALL) || (f.addr == myAddr());
  if (!forMe) return;

  if (f.cmd == proto::CMD_REPORT) {
    char s[40];
    snprintf(s, sizeof(s), "%d,%d,%d,%.1f",
             (int)role_, awake_ ? 1 : 0, playing_ ? 1 : 0, tempmonGetTemp());
    link_.send(proto::ADDR_LEADER, proto::CMD_STATUS, s);
  } else {
    dispatch(f.cmd, f.arg);
  }
}

// ===================== USB console =====================
void Controller::usbPoll() {
  if (!Serial.available()) return;

  if (Serial.peek() == ':') {
    char line[48]; int i = 0;
    Serial.read();
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
        rtc_.setTime(Y, Mo, D, H, Mi, S);
        Serial.print("RTC set "); rtc_.print(Serial);
      } else Serial.println("Usage: :settime YYYY MM DD HH MM SS");
    } else if (strncmp(line, "synctime", 8) == 0) {
      rtc_.syncToCompile();
      Serial.print("RTC = compile time "); rtc_.print(Serial);
    } else if (strncmp(line, "time", 4) == 0) {
      rtc_.print(Serial);
    } else {
      Serial.println("Unknown : command");
    }
    return;
  }

  char k = Serial.read();
  if (k <= 32) return;
  if (role_ == Role::LEADER &&
      (k == proto::CMD_PLAY || k == proto::CMD_STOP || k == proto::CMD_WAKE ||
       k == proto::CMD_SLEEP || k == proto::CMD_VOLUP || k == proto::CMD_VOLDN ||
       k == proto::CMD_PWMUP || k == proto::CMD_PWMDN || k == proto::CMD_REBOOT)) {
    link_.send(proto::ADDR_ALL, k, "");
  }
  dispatch(k, "");
}

// ===================== leader / follower =====================
void Controller::leaderTask() {
  // wake/sleep from the clock, every 30 s
  if (statusTimer_ >= 30000) {
    statusTimer_ = 0;
    bool shouldWake = rtc_.isActiveHour(cfg::START_HOUR, cfg::END_HOUR);
    if (shouldWake != awake_) {
      link_.send(proto::ADDR_ALL, shouldWake ? proto::CMD_WAKE : proto::CMD_SLEEP, "");
      applyAwake(shouldWake);
    }
  }

  // heartbeat carries awake-state so late followers self-sync
  if (heartTimer_ >= 5000) {
    heartTimer_ = 0;
    link_.send(proto::ADDR_ALL, proto::CMD_HEART, awake_ ? "1" : "0");
  }

  // synchronized rounds
  if (awake_) {
    if (!audio_.isPlaying()) {
      if (gapTimer_ >= cfg::LOOP_GAP_MS) {
        link_.send(proto::ADDR_ALL, proto::CMD_PLAY, "");
        playRound();
        gapTimer_ = 0;
      }
      leds_.show(status::AWAKE);
      analogWrite(cfg::PWM_PIN, 0);
    } else {
      writePwm();
      if (knob_) volumeFromKnob();
      leds_.show(status::PLAYING);
      gapTimer_ = 0;

      // 1 Hz audio-health trace. mem approaching the AudioMemory() ceiling,
      // or t stalling between lines, means real playback starvation; flat
      // mem through an audible episode means the fault is not buffering.
      if (dbgTimer_ >= 1000) {
        dbgTimer_ = 0;
        Serial.printf("t=%lu mem=%u cpu=%.1f\n",
                      (unsigned long)audio_.posMs(),
                      AudioEngine::memMax(), AudioEngine::cpuMax());
        AudioEngine::statsReset();
      }
    }
  } else {
    if (audio_.isPlaying()) audio_.stop();
    leds_.show(status::SLEEP);
    analogWrite(cfg::PWM_PIN, 0);
  }

  // poll followers, staggered so only one replies at a time
  if (pollTimer_ >= 20000) {
    pollTimer_ = 0;
    link_.send(proto::ADDR_SMALL, proto::CMD_REPORT, "");
    feedWdt(); delay(150);
    link_.send(proto::ADDR_SEASHELL, proto::CMD_REPORT, "");
  }
}

void Controller::followerTask() {
  bool stale = linkTimer_ > 90000;
  if (awake_) {
    if (audio_.isPlaying()) { writePwm(); leds_.show(status::PLAYING); }
    else { leds_.show(stale ? status::LINK_ERR : status::AWAKE); analogWrite(cfg::PWM_PIN, 0); }
  } else {
    if (audio_.isPlaying()) audio_.stop();
    leds_.show(stale ? status::LINK_ERR : status::SLEEP);
    analogWrite(cfg::PWM_PIN, 0);
  }
}

// ===================== report / reboot =====================
void Controller::report() {
  Serial.println(F("\n----- REPORT -----"));
  Serial.print("Role "); Serial.println((int)role_);
  Serial.print("File "); Serial.println(file_);
  if (role_ == Role::LEADER) { Serial.print("RTC "); rtc_.print(Serial); }
  Serial.print("Awake "); Serial.println(awake_ ? "YES" : "NO");
  Serial.print("Playing "); Serial.println(playing_ ? "YES" : "NO");
  Serial.print("Volume "); Serial.println(audio_.volume());
  Serial.print("PWM range "); Serial.println(rangePwm_);
  Serial.print("Track iter "); Serial.println(trackIter_);
  Serial.print("CPU temp "); Serial.print(tempmonGetTemp()); Serial.println(" C");
  Serial.println(F("----- END -----\n"));
}

void Controller::reboot() {
  Serial.println("Rebooting shortly.");
  if (role_ == Role::LEADER) link_.send(proto::ADDR_ALL, proto::CMD_REBOOT, "");
  if (awake_) relays_.sleep();
  awake_ = false;
  leds_.show(status::REBOOT);
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) feedWdt();
  SCB_AIRCR = 0x05FA0004;        // system reset
}
