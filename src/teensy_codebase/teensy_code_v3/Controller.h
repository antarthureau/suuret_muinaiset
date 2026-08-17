/**
 * Controller.h - orchestrates the whole unit.
 *
 * Owns every subsystem, detects the role from the strap pins, runs the
 * leader or follower state machine, dispatches commands from USB and from
 * the Serial3 link, and feeds the watchdog. One global Controller is
 * created in main; setup()/loop() forward to it.
 */
#ifndef SM_CONTROLLER_H
#define SM_CONTROLLER_H

#include <Arduino.h>
#include <elapsedMillis.h>
#include "config.h"
#include "StatusLeds.h"
#include "Relays.h"
#include "AudioEngine.h"
#include "RtcClock.h"
#include "SerialLink.h"

#if USE_WATCHDOG
#include <Watchdog_t4.h>
#endif

class Controller {
public:
  Controller() : link_(Serial3) {}
  void setup();
  void loop();

private:
  // role
  Role        detectRole();
  const char *fileForRole() const;
  char        myAddr() const;

  // behaviour
  void feedWdt();
  void applyAwake(bool target);
  void playRound();
  void writePwm();
  void volumeFromKnob();
  void dispatch(char cmd, const char *arg);
  void handleFrame(const Frame &f);
  void usbPoll();
  void leaderTask();
  void followerTask();
  void report();
  void reboot();

  // subsystems
  StatusLeds  leds_;
  Relays      relays_;
  AudioEngine audio_;
  RtcClock    rtc_;
  SerialLink  link_;

  // state
  Role  role_  = Role::LEADER;
  const char *file_ = "";
  bool  awake_   = false;
  bool  playing_ = false;
  int   trackIter_ = 0;
  int   rangePwm_  = 255;
  bool  knob_      = false;

  // timers
  elapsedMillis statusTimer_, heartTimer_, pollTimer_;
  elapsedMillis linkTimer_, gapTimer_, retryTimer_, pwmTimer_;

#if USE_WATCHDOG
  WDT_T4<WDT1> wdt_;
#endif
};

#endif // SM_CONTROLLER_H
