/**
 * Controller.h - orchestrates the whole unit.
 *
 * ---------------------------------------------------------------------------
 * RESPONSIBILITY
 * ---------------------------------------------------------------------------
 * Owns every subsystem, detects the role from the strap pins at boot, runs
 * either the leader or the follower state machine, dispatches commands
 * arriving from USB and from the Serial3 link, and feeds the watchdog. One
 * global Controller is created in the sketch; setup() and loop() forward to it.
 *
 * ---------------------------------------------------------------------------
 * THE TWO STATE MACHINES
 * ---------------------------------------------------------------------------
 * LEADER (LONG). Holds the only clock and the only schedule. Every 30 s it
 * asks the RTC whether the installation should be awake and, on a change,
 * broadcasts wake or sleep before applying it locally. Every 5 s it repeats
 * the current awake state as a heartbeat, so a follower that rebooted mid-cycle
 * resynchronizes without anyone intervening. While awake it drives rounds: when
 * nothing is playing and the inter-round gap has elapsed, it broadcasts PLAY
 * and starts its own file. Every 20 s it asks each follower for status, one
 * after the other rather than at once.
 *
 * FOLLOWER (SMALL, SEASHELL). Holds no schedule and no timers of its own. It
 * acts on what arrives, reports when addressed, and tracks how long it has been
 * since the last heartbeat. Past cfg::LINK_STALE_MS it keeps doing whatever it
 * was doing but shows status::LINK_ERR, so a broken cable is visible on the LED
 * array without changing the behaviour of the piece.
 *
 * Rounds are re-broadcast every cycle rather than started once and left to run,
 * so the three units resynchronize continuously instead of drifting apart over
 * a day.
 *
 * ---------------------------------------------------------------------------
 * NON-BLOCKING BY DESIGN
 * ---------------------------------------------------------------------------
 * loop() must keep running: it services the light envelope, drains the serial
 * link and feeds the watchdog. Everything periodic is driven by elapsedMillis
 * timers rather than delays, including the stagger between the two status
 * requests. The one deliberate exception is relay sequencing, which blocks for
 * about a second on a schedule transition; see Relays.h for why that is
 * accepted.
 *
 * A failed init is never fatal and never blocks. A unit with no card or no
 * codec still boots, still shows a status code and still answers on the link,
 * so it can be diagnosed from the other end of the installation.
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
  /* ---- role ---- */

  /**
   * Read the hardware ID straps once at boot. Pins use the internal pulldown,
   * so a broken or unconnected strap reads LOW and is reported rather than
   * floating to a random role.
   */
  Role        detectRole();
  const char *fileForRole() const;
  char        myAddr() const;

  /* ---- behaviour ---- */

  void feedWdt();

  /** Apply an awake/asleep transition. Edge-triggered; a repeat is a no-op. */
  void applyAwake(bool target);

  /** Start this unit's file from the beginning and count the round. */
  void playRound();

  /** Sample the envelope and update the LED strip. Rate-limited internally. */
  void writePwm();

  /** Read the volume pot, with a deadband so noise does not chatter the codec. */
  void volumeFromKnob();

  /** Execute one command, whatever its source. */
  void dispatch(char cmd, const char *arg);

  /** Decide whether an incoming frame applies to this unit, then act. */
  void handleFrame(const Frame &f);

  /** Service the USB console: single keys and ':' line commands. */
  void usbPoll();

  void leaderTask();
  void followerTask();
  void report();
  void reboot();

  /* ---- subsystems ---- */
  StatusLeds  leds_;
  Relays      relays_;
  AudioEngine audio_;
  RtcClock    rtc_;        // leader only; harmless and inert on followers
  SerialLink  link_;

  /* ---- state ---- */
  Role        role_      = Role::LEADER;
  const char *file_      = "";
  bool        awake_     = false;
  bool        playing_   = false;
  int         trackIter_ = 0;
  int         rangePwm_  = 255;    // scales the envelope into the PWM duty
  bool        knob_      = false;  // volume pot active
  float       lastKnobRaw_ = -1.0f;

  /**
   * Follower status polling runs as a two-step sequence rather than a delay:
   * stage 0 waits for the poll interval and addresses SMALL, stage 1 waits out
   * the stagger and addresses SEASHELL. Only one follower is ever asked to
   * transmit at a time, and the loop keeps running throughout.
   */
  uint8_t pollStage_ = 0;

  /* ---- timers ---- */
  elapsedMillis statusTimer_;    // schedule check
  elapsedMillis heartTimer_;     // heartbeat broadcast
  elapsedMillis pollTimer_;      // status poll interval
  elapsedMillis pollGapTimer_;   // stagger between the two followers
  elapsedMillis linkTimer_;      // since last heartbeat (followers)
  elapsedMillis gapTimer_;       // silence between rounds
  elapsedMillis retryTimer_;     // codec/SD recovery attempts
  elapsedMillis pwmTimer_;       // light refresh
  elapsedMillis knobTimer_;      // volume pot sampling
  elapsedMillis dbgTimer_;       // 1 Hz audio-health trace while playing

#if USE_WATCHDOG
  WDT_T4<WDOG1> wdt_;
#endif
};

#endif  // SM_CONTROLLER_H
