/**
 * Controller.cpp - role detection and the leader/follower state machines.
 * See Controller.h for the architecture and why nothing here blocks.
 */
#include "Controller.h"
#include <math.h>

/* ===========================================================================
 * ROLE
 *
 * Decided once at boot from the hardware ID straps and never revisited. Every
 * per-unit difference in behaviour derives from this one reading.
 * ========================================================================= */

Role Controller::detectRole() {
  /*
   * Internal pulldowns matter here. With a plain INPUT a disconnected strap
   * floats and can read HIGH, which would let a follower decide it is the
   * leader - putting two transmitters on the shared back-channel. Pulled down,
   * a broken strap reads LOW and falls through to the reported default.
   */
  pinMode(cfg::PIN_LONG,     INPUT_PULLDOWN);
  pinMode(cfg::PIN_SMALL,    INPUT_PULLDOWN);
  pinMode(cfg::PIN_SEASHELL, INPUT_PULLDOWN);

  if (digitalRead(cfg::PIN_LONG)     == HIGH) return Role::LEADER;
  if (digitalRead(cfg::PIN_SMALL)    == HIGH) return Role::SMALL;
  if (digitalRead(cfg::PIN_SEASHELL) == HIGH) return Role::SEASHELL;

  /*
   * No strap read high. Defaulting to leader keeps a single unit on a bench
   * useful, but on site it means a strap has failed - so say so loudly rather
   * than letting it pass as normal.
   */
  Serial.println(F("WARNING: no ID strap detected, defaulting to LEADER"));
  return Role::LEADER;
}

/**
 * Return the WAV file name for this unit's role. The leader plays a long
 * file, the followers play their own short files. The file is never changed
 * after boot, so the leader can schedule rounds without asking the followers.
 * @param role The role to query.
 * @return The file name for the role, or "" if the role is unknown.
 */
const char *Controller::fileForRole() const {
  switch (role_) {
    case Role::SMALL:    return cfg::FILE_SMALL;
    case Role::SEASHELL: return cfg::FILE_SEASHELL;
    default:             return cfg::FILE_LONG;
  }
}

/**
 * Return the address for this unit's role. The leader is '0', the followers
 * are '1' and '2'. The address is never changed after boot, so the leader
 * can schedule rounds without asking the followers. The address is used in
 * the back-channel protocol to decide whether an incoming frame applies to
 * this unit.
 * @param role The role to query.
 * @return The address for the role, or '0' if the role is unknown.
 */
char Controller::myAddr() const {
  switch (role_) {
    case Role::SMALL:    return proto::ADDR_SMALL;
    case Role::SEASHELL: return proto::ADDR_SEASHELL;
    default:             return proto::ADDR_LEADER;
  }
}

/* ===========================================================================
 * WATCHDOG
 * ========================================================================= */

 /**
  * Feed the watchdog. The watchdog is enabled at boot and fed throughout
  * loop() and during any blocking operations, so a slow subsystem cannot
  * trip it. The watchdog is disabled at compile time for development builds
  * to avoid a crash loop when stepping through code.
  */
void Controller::feedWdt() {
#if USE_WATCHDOG
  wdt_.feed();
#endif
}

/* ===========================================================================
 * SETUP
 * ========================================================================= */

 /**
  * Initialize the controller and all subsystems. Called once at boot.
  * The watchdog is enabled at boot and fed throughout setup() and loop(),
  * so a slow subsystem cannot trip it. The watchdog is disabled at compile
  * time for development builds to avoid a crash loop when stepping through code.
  */
void Controller::setup() {
  /*
   * Baud is ignored on the Teensy's USB serial, which always runs at native
   * USB speed. It matters for Serial3, where 9600 is slow but ample: the
   * longest frame is well under 64 bytes and the link carries a few frames a
   * minute over a long buried cable, where slower is more forgiving.
   */
  Serial.begin(9600);
  link_.begin(9600);

  /* Surviving crash data from the previous run, if any. */
  if (CrashReport) { Serial.print("Crash: "); Serial.println(CrashReport); }

  role_ = detectRole();
  file_ = fileForRole();
  Serial.print("Role "); Serial.print((int)role_);
  Serial.print(" file "); Serial.println(file_);

  leds_.begin();
  relays_.begin();

  /*
   * Light output. The frequency is set above the audible band before anything
   * is written: the Teensy 4 default of roughly 4.5 kHz is heard as a whine
   * through the shared ground that tracks LED brightness.
   */
  pinMode(cfg::PWM_PIN, OUTPUT);
  analogWriteFrequency(cfg::PWM_PIN, cfg::PWM_FREQ_HZ);
  analogWriteResolution(8);
  analogWrite(cfg::PWM_PIN, 0);

  /*
   * Init is non-fatal: report what failed and carry on. The LED array is not
   * written here - announceFaults() does it at the end of setup, because
   * anything shown now would be overwritten by the first pass of loop().
   */
  audio_.begin(cfg::DEFAULT_VOLUME);
  if (!audio_.codecReady()) Serial.println("codec fail");
  if (!audio_.sdReady())    Serial.println("SD fail");

  if (role_ == Role::LEADER) {
    if (!rtc_.begin()) Serial.println("RTC fail");
  }

#if USE_WATCHDOG
  WDT_timings_t t;
  t.trigger = cfg::WDT_TIMEOUT_S;
  wdt_.begin(t);
#endif

  Serial.println("Setup complete.");

  /* Last thing before loop() takes over the display. */
  announceFaults();
}

/* ===========================================================================
 * BOOT FAULT ANNOUNCEMENT
 *
 * Blink each fault found during setup on the LED array, long enough for a
 * technician standing at an open enclosure to read it, then hand the display
 * over to loop().
 *
 * Faults are announced rather than latched, so a unit that has been running
 * for hours shows its normal state. To re-read them on site, reboot the unit
 * and watch the array during startup.
 *
 * This blocks, deliberately: nothing is playing yet and no round has started,
 * so the only cost is delaying the first loop() by a few seconds. The watchdog
 * is fed throughout.
 * ========================================================================= */

 /**
  * Blink any faults found during setup on the status LEDs, then hand the display
  * back to loop(). Called once, at the end of setup().
  */
void Controller::announceFaults() {
  int faults[3];
  int n = 0;

  if (!audio_.codecReady())                 faults[n++] = status::CODEC_ERR;
  if (!audio_.sdReady())                    faults[n++] = status::SD_ERR;
  if (role_ == Role::LEADER && !rtc_.ok())  faults[n++] = status::RTC_ERR;

  if (n == 0) return;                       // healthy unit: say nothing

  Serial.print("Announcing ");
  Serial.print(n);
  Serial.println(" fault code(s) on the LED array.");

  for (int i = 0; i < n; i++) {
    bool on = true;
    leds_.show(faults[i]);

    elapsedMillis held  = 0;
    elapsedMillis phase = 0;
    while (held < cfg::FAULT_BLINK_MS) {
      feedWdt();
      if (phase >= cfg::FAULT_BLINK_HALF_MS) {
        phase = 0;
        on = !on;
        leds_.show(on ? faults[i] : 0);     // code 0 is all LEDs off
      }
    }
  }

  leds_.show(0);
}

/* ===========================================================================
 * LOOP
 *
 * Order matters: feed the watchdog first so a slow subsystem downstream cannot
 * trip it, then recover, then drain inputs, then run the role's state machine.
 * ========================================================================= */

 /**
  * Main loop. Called repeatedly after setup() completes. The watchdog is fed
  * first, so a slow subsystem cannot trip it. The watchdog is disabled at
  * compile time for development builds to avoid a crash loop when stepping
  * through code.
  */
void Controller::loop() {
  feedWdt();

  /*
   * Recover a codec or card that did not come up at boot. Suppressed while
   * audio is playing, because SD.begin() re-initializes the very card the
   * audio interrupt is streaming from; AudioEngine caps the attempts.
   */
  if ((!audio_.codecReady() || !audio_.sdReady()) &&
      !audio_.isPlaying() &&
      retryTimer_ >= cfg::RETRY_INTERVAL_MS) {
    retryTimer_ = 0;
    audio_.retryInit();
  }

  //USB console polling is first so a key typed on the console and a frame arriving on Serial3 cannot diverge in behaviour.
  usbPoll();

  /* Drain every frame that arrived since the last pass. */
  Frame f;
  while (link_.poll(f)) handleFrame(f);

  playing_ = audio_.isPlaying();

  if (role_ == Role::LEADER) leaderTask();
  else                       followerTask();
}

/* ===========================================================================
 * BEHAVIOUR
 * ========================================================================= */

/**
 * Apply an awake/asleep transition. Edge-triggered; a repeat is a no-op. The
 * relay sequencing blocks for about a second. Feed the watchdog on the way in so the transition cannot be mistaken for a hang.
 * @param target True to wake, false to sleep.
 */
void Controller::applyAwake(bool target) {
  if (target == awake_) return;          // edge-triggered

  /*
   * Relay sequencing blocks for about a second. Feed the watchdog on the way
   * in so the transition cannot be mistaken for a hang.
   */
  feedWdt();
  if (target) relays_.wake(); else relays_.sleep();

  awake_ = target;
  if (target)  trackIter_ = 0;
  if (!target) audio_.stop();
}

/**
 * Start this unit's file from the beginning and count the round. The file is
 * never changed after boot, so the leader can schedule rounds without asking
 * the followers.
 */
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

/** 
 * Sample the envelope and update the LED strip. Rate-limited internally.
 * The envelope is clamped to the range 0-255, then scaled by range and written to the PWM pin.
 * The PWM pin is configured for 8-bit resolution and a frequency above the audible range.
 */
void Controller::writePwm() {
  if (pwmTimer_ < cfg::PWM_UPDATE_MS) return;
  pwmTimer_ = 0;

  float lv = audio_.level(cfg::PEAK_MODE);
  if (lv < 0.0f) return;                 // no new analyzer window yet

  int v = (int)(lv * rangePwm_);
  if (v < 0)   v = 0;
  if (v > 255) v = 255;
  analogWrite(cfg::PWM_PIN, v);
}

/**
 * Read the volume pot, with a deadband so noise does not chatter the codec.
 * The pot is read every VOL_POLL_MS milliseconds, and the value is quantized to
 * 0.1 steps before being sent to the codec and broadcasted over Serial3.
 */
void Controller::volumeFromKnob() {
  if (knobTimer_ < cfg::VOL_POLL_MS) return;
  knobTimer_ = 0;

  /*
   * Two filters before anything is committed. The deadband on the raw reading
   * absorbs electrical noise on a long pot lead; without it a reading sitting
   * near a quantization boundary produces an I2C codec write and a Serial3
   * broadcast every 50 ms, heard as zipper noise. The quantization to 0.1
   * steps then keeps the reported value readable.
   */
  float raw = analogRead(cfg::VOL_POT) / 1023.0f;
  if (fabsf(raw - lastKnobRaw_) < cfg::VOL_DEADBAND) return;
  lastKnobRaw_ = raw;

  float q = roundf(raw * 10.0f) / 10.0f;
  if (fabsf(q - audio_.volume()) < 0.001f) return;

  audio_.setVolume(q);
  Serial.print("Volume "); Serial.println(audio_.volume());

  char arg[8];
  snprintf(arg, sizeof(arg), "%.1f", audio_.volume());
  link_.send(proto::ADDR_ALL, proto::CMD_VOL, arg);
}

/* ===========================================================================
 * COMMAND DISPATCH
 *
 * One entry point for both sources, so a key typed on the USB console and a
 * frame arriving on Serial3 cannot diverge in behaviour.
 * ========================================================================= */

 /**
  * Execute one command, whatever its source. The command is a single character,
  * optionally followed by a string argument. The command is dispatched to the
  * appropriate handler, which may be a no-op if the command is not relevant to
  * this unit's role or state.
  * @param cmd The command character.
  * @param arg The optional argument string, or nullptr if none. 
  */
void Controller::dispatch(char cmd, const char *arg) {
  switch (cmd) {
    case proto::CMD_PLAY:
      playRound();
      break;

    case proto::CMD_STOP:
      audio_.stop();
      playing_ = false;
      analogWrite(cfg::PWM_PIN, 0);
      break;

    case proto::CMD_WAKE:  applyAwake(true);  break;
    case proto::CMD_SLEEP: applyAwake(false); break;

    case proto::CMD_REPORT: report(); break;
    case proto::CMD_REBOOT: reboot(); break;

    case proto::CMD_VOL:
      if (arg && *arg) {
        audio_.setVolume(atof(arg));
        Serial.print("Volume "); Serial.println(audio_.volume());
      }
      break;

    case proto::CMD_VOLUP:
      audio_.setVolume(audio_.volume() + 0.1f);
      Serial.print("Volume "); Serial.println(audio_.volume());
      break;

    case proto::CMD_VOLDN:
      audio_.setVolume(audio_.volume() - 0.1f);
      Serial.print("Volume "); Serial.println(audio_.volume());
      break;

    case proto::CMD_PWMUP:
      rangePwm_ += 25; if (rangePwm_ > 255) rangePwm_ = 255;
      Serial.print("PWM "); Serial.println(rangePwm_);
      break;

    case proto::CMD_PWMDN:
      rangePwm_ -= 25; if (rangePwm_ < 0) rangePwm_ = 0;
      Serial.print("PWM "); Serial.println(rangePwm_);
      break;

    case proto::CMD_KNOB:
      knob_ = !knob_;
      lastKnobRaw_ = -1.0f;      // force the next reading to commit
      Serial.print("Knob "); Serial.println(knob_ ? "ON" : "OFF");
      break;

    case proto::CMD_HEART:
      /*
       * Heartbeat carries the leader's awake state, so a follower that missed
       * the original wake or sleep converges within five seconds instead of
       * staying out of step until the next schedule change.
       */
      if (role_ != Role::LEADER && arg && *arg) {
        linkTimer_ = 0;
        applyAwake(arg[0] == '1');
      }
      break;

    case proto::CMD_HELP:
      Serial.println(F("\nKeys: P play  X stop  W wake  S sleep  R report  B reboot"));
      Serial.println(F("      + - volume   > < PWM   K knob"));
      Serial.println(F("Clock:  :settime YYYY MM DD HH MM SS   :synctime   :time"));
      Serial.println(F("Status (leader): :small   :seashell   :status"));
      break;

    default:
      break;
  }
}

/* ===========================================================================
 * INCOMING FRAMES
 * ========================================================================= */

 /**
  * Decide whether an incoming frame applies to this unit, then act. The frame
  * is addressed to either all units or a specific unit. The leader only ever
  * listens for status replies, so it ignores commands from the bus. A follower
  * may be addressed directly or as part of a broadcast. The frame is acted on
  * immediately, so a reflected frame cannot drive the installation.
  * @param f The incoming frame.
  */
void Controller::handleFrame(const Frame &f) {
  /*
   * The leader only ever listens for status replies. It must not act on
   * commands from the bus, or a reflected frame could drive the installation.
   */
  if (role_ == Role::LEADER) {
    if (f.addr == proto::ADDR_LEADER && f.cmd == proto::CMD_STATUS) {
      /*
       * Followers send "role,awake,playing,temp". Label the fields rather than
       * printing raw CSV: this is what a technician reads on site, and the
       * automatic poll prints it unprompted every 20 s.
       */
      int fr = -1, fa = -1, fp = -1;
      float ft = 0.0f;
      if (sscanf(f.arg, "%d,%d,%d,%f", &fr, &fa, &fp, &ft) == 4) {
        Serial.print("Status from ");
        Serial.print(fr == (int)Role::SMALL ? "SMALL" :
                     fr == (int)Role::SEASHELL ? "SEASHELL" : "?");
        Serial.print(": awake ");   Serial.print(fa ? "YES" : "NO");
        Serial.print(", playing "); Serial.print(fp ? "YES" : "NO");
        Serial.print(", CPU ");     Serial.print(ft);
        Serial.println(" C");
      } else {
        Serial.print("Follower status (unparsed): "); Serial.println(f.arg);
      }
    }
    return;
  }

  bool forMe = (f.addr == proto::ADDR_ALL) || (f.addr == myAddr());
  if (!forMe) return;

  if (f.cmd == proto::CMD_REPORT) {
    // Reply only when individually addressed; see SerialLink.h on collisions.
    char s[40];
    snprintf(s, sizeof(s), "%d,%d,%d,%.1f",
             (int)role_, awake_ ? 1 : 0, playing_ ? 1 : 0, tempmonGetTemp());
    link_.send(proto::ADDR_LEADER, proto::CMD_STATUS, s);
  } else {
    dispatch(f.cmd, f.arg);
  }
}

/* ===========================================================================
 * USB CONSOLE
 *
 * Single keys act immediately. A leading ':' introduces a line command, the
 * only place this firmware waits on input - bounded by a short inter-character
 * timeout so a half-typed line cannot stall the loop.
 * ========================================================================= */

  /**
  * Poll the USB console for input.
  */
void Controller::usbPoll() {
  if (!Serial.available()) return;

  if (Serial.peek() == ':') {
    char line[48];
    int i = 0;
    Serial.read();                       // consume ':'
    unsigned long t0 = millis();

    while (i < (int)sizeof(line) - 1) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        line[i++] = c;
        t0 = millis();
      } else if (millis() - t0 > 200) {
        break;                           // typing stopped; act on what we have
      }
    }
    line[i] = 0;

    // Clock commands are leader business; on a follower rtc_ is inert.
    if (strncmp(line, "settime", 7) == 0) {
      int Y, Mo, D, H, Mi, S;
      if (sscanf(line + 7, "%d %d %d %d %d %d", &Y, &Mo, &D, &H, &Mi, &S) == 6) {
        rtc_.setTime(Y, Mo, D, H, Mi, S);
        Serial.print("RTC set "); rtc_.print(Serial);
      } else {
        Serial.println("Usage: :settime YYYY MM DD HH MM SS");
      }
    } else if (strncmp(line, "synctime", 8) == 0) {
      rtc_.syncToCompile();
      Serial.print("RTC = compile time "); rtc_.print(Serial);
    } else if (strncmp(line, "time", 4) == 0) {
      rtc_.print(Serial);

    /*
     * On-demand status requests, leader only. V2 had these as :small and
     * :seashell and the names are kept, but the mechanism is different: the
     * frame is addressed, so the follower that was not asked simply stays
     * quiet. V2 had to call Serial3.end() on the other unit for 250 ms to
     * keep it off the shared back-channel.
     *
     * Replies are asynchronous. They arrive on Serial3 a moment later and
     * print from handleFrame(), not from here.
     */
    } else if (strncmp(line, "small", 5) == 0) {
      if (role_ != Role::LEADER) { Serial.println("Leader only."); return; }
      link_.send(proto::ADDR_SMALL, proto::CMD_REPORT, "");
      Serial.println("Status requested from SMALL.");
    } else if (strncmp(line, "seashell", 8) == 0) {
      if (role_ != Role::LEADER) { Serial.println("Leader only."); return; }
      link_.send(proto::ADDR_SEASHELL, proto::CMD_REPORT, "");
      Serial.println("Status requested from SEASHELL.");
    } else if (strncmp(line, "status", 6) == 0) {
      if (role_ != Role::LEADER) { Serial.println("Leader only."); return; }
      /*
       * Ask both, reusing the staggered poll sequence in leaderTask() rather
       * than sending two frames now - the followers share one back-channel and
       * must not answer at the same time. Arming the timer makes the sequence
       * start on the next pass of loop().
       */
      pollTimer_ = cfg::REPORT_POLL_MS;
      pollStage_ = 0;
      Serial.println("Status requested from both followers.");

    } else {
      Serial.println("Unknown : command");
    }
    return;
  }

  char k = Serial.read();
  if (k <= 32) return;                   // ignore whitespace and control bytes

  /*
   * On the leader, keys that should affect the whole installation are relayed
   * before being applied locally. Note this means volume and PWM keys typed on
   * the leader move all three units together.
   */
  if (role_ == Role::LEADER &&
      (k == proto::CMD_PLAY  || k == proto::CMD_STOP   || k == proto::CMD_WAKE ||
       k == proto::CMD_SLEEP || k == proto::CMD_VOLUP  || k == proto::CMD_VOLDN ||
       k == proto::CMD_PWMUP || k == proto::CMD_PWMDN  || k == proto::CMD_REBOOT)) {
    link_.send(proto::ADDR_ALL, k, "");
  }

  dispatch(k, "");
}

/* ===========================================================================
 * LEADER
 * ========================================================================= */

 /** 
  * Run the leader state machine. The leader schedules rounds, broadcasts awake/asleep transitions,
  * and restates its awake state so late or rebooted followers self-sync.
  * The leader also polls the followers for status, staggered so only one follower transmits at a time.
  */
void Controller::leaderTask() {
  // Schedule. Broadcast before applying, so followers switch with us.
  if (statusTimer_ >= cfg::SCHEDULE_POLL_MS) {
    statusTimer_ = 0;
    bool shouldWake = rtc_.isActiveHour(cfg::START_HOUR, cfg::END_HOUR);
    if (shouldWake != awake_) {
      link_.send(proto::ADDR_ALL,
                 shouldWake ? proto::CMD_WAKE : proto::CMD_SLEEP, "");
      applyAwake(shouldWake);
    }
  }

  // Heartbeat: restates awake state so late or rebooted followers self-sync. 
  if (heartTimer_ >= cfg::HEARTBEAT_MS) {
    heartTimer_ = 0;
    link_.send(proto::ADDR_ALL, proto::CMD_HEART, awake_ ? "1" : "0");
  }

  if (awake_) {
    if (!audio_.isPlaying()) {
      /*
       * Between rounds. The broadcast goes out first and the leader starts its
       * own file immediately after; at 9600 baud the followers begin a few
       * milliseconds later, which is below the threshold of noticing across
       * three sculptures spread over a site.
       */
      if (gapTimer_ >= cfg::LOOP_GAP_MS) {
        link_.send(proto::ADDR_ALL, proto::CMD_PLAY, "");
        playRound();
        gapTimer_ = 0;
      }
      leds_.show(status::AWAKE);
      analogWrite(cfg::PWM_PIN, 0);
    } else {
      /* Playing. */
      writePwm();
      if (knob_) volumeFromKnob();
      leds_.show(status::PLAYING);
      gapTimer_ = 0;

#if AUDIO_TRACE
      /*
       * Audio-health trace, leader only, once a second while playing.
       * Compiled out unless AUDIO_TRACE is 1 in config.h, so the USB console
       * stays quiet in normal field operation.
       *
       * mem climbing toward cfg::AUDIO_MEMORY_BLOCKS, or t failing to advance
       * by roughly 1000 between lines, means the SD path is starving. Both
       * staying healthy through an audible fault means the digital side is
       * delivering correctly and the problem is downstream in the analog chain.
       */
      if (dbgTimer_ >= cfg::DEBUG_TRACE_MS) {
        dbgTimer_ = 0;
        Serial.printf("t=%lu mem=%u cpu=%.1f\n",
                      (unsigned long)audio_.posMs(),
                      AudioEngine::memMax(), AudioEngine::cpuMax());
        AudioEngine::statsReset();
      }
#endif
    }
  } else {
    if (audio_.isPlaying()) audio_.stop();
    leds_.show(status::SLEEP);
    analogWrite(cfg::PWM_PIN, 0);
  }

  /*
   * Status polling, staggered so only one follower transmits at a time. Run as
   * a two-step sequence rather than a delay, so the light envelope keeps
   * updating while the gap elapses.
   */
  if (pollStage_ == 0) {
    if (pollTimer_ >= cfg::REPORT_POLL_MS) {
      pollTimer_    = 0;
      pollGapTimer_ = 0;
      pollStage_    = 1;
      link_.send(proto::ADDR_SMALL, proto::CMD_REPORT, "");
    }
  } else {
    if (pollGapTimer_ >= cfg::REPORT_STAGGER_MS) {
      pollStage_ = 0;
      link_.send(proto::ADDR_SEASHELL, proto::CMD_REPORT, "");
    }
  }
}

/* ===========================================================================
 * FOLLOWER
 *
 * No schedule, no round timing. It renders whatever state the leader last put
 * it in, and reports a stale link without changing its behaviour.
 * ========================================================================= */

 /**
  * Run the follower state machine. The follower renders whatever state the leader last put it in,
  * and reports a stale link without changing its behaviour.
  */
void Controller::followerTask() {
  bool stale = linkTimer_ > cfg::LINK_STALE_MS;

  if (awake_) {
    if (audio_.isPlaying()) {
      writePwm();
      leds_.show(status::PLAYING);
    } else {
      leds_.show(stale ? status::LINK_ERR : status::AWAKE);
      analogWrite(cfg::PWM_PIN, 0);
    }
  } else {
    if (audio_.isPlaying()) audio_.stop();
    leds_.show(stale ? status::LINK_ERR : status::SLEEP);
    analogWrite(cfg::PWM_PIN, 0);
  }
}

/* ===========================================================================
 * REPORT / REBOOT
 * ========================================================================= */

 /**
  * Report the unit's current status.
  */
void Controller::report() {
  Serial.println(F("\n----- REPORT -----"));
  Serial.print("Role ");       Serial.println((int)role_);
  Serial.print("File ");       Serial.println(file_);
  if (role_ == Role::LEADER) { Serial.print("RTC "); rtc_.print(Serial); }
  Serial.print("Awake ");      Serial.println(awake_ ? "YES" : "NO");
  Serial.print("Playing ");    Serial.println(playing_ ? "YES" : "NO");
  Serial.print("Volume ");     Serial.println(audio_.volume());
  Serial.print("PWM range ");  Serial.println(rangePwm_);
  Serial.print("Track iter "); Serial.println(trackIter_);
  Serial.print("Codec ");      Serial.println(audio_.codecReady() ? "OK" : "FAIL");
  Serial.print("SD ");         Serial.println(audio_.sdReady() ? "OK" : "FAIL");
  Serial.print("CPU temp ");   Serial.print(tempmonGetTemp()); Serial.println(" C");
  Serial.println(F("----- END -----\n"));
}

/**
 * Reboot the unit.
 */
void Controller::reboot() {
  Serial.println("Rebooting shortly.");

  // Relay the command before resetting, or the followers keep running alone.
  if (role_ == Role::LEADER) link_.send(proto::ADDR_ALL, proto::CMD_REBOOT, "");

  // Power down cleanly so the reset does not thump through the speaker.
  if (awake_) relays_.sleep();
  awake_ = false;
  leds_.show(status::REBOOT);

  // Hold long enough for the LED code to be readable, feeding the watchdog.
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) feedWdt();

  SCB_AIRCR = 0x05FA0004;        // ARM system reset
}
