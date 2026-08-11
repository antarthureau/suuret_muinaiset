/**
 * Relays.h - amplifier and speaker power sequencing.
 *
 * PURPOSE
 *   Two relays per unit. REL_AMP switches mains through to the 36V PSU that
 *   feeds the amplifier; REL_SPK connects the amplifier output to the speaker.
 *   Sleeping the installation cuts the 36V rail entirely rather than leaving a
 *   150W amplifier idling overnight in a sealed outdoor box.
 *
 * WHY THE ORDER MATTERS
 *   wake() energizes the amplifier first and only then connects the speaker;
 *   sleep() disconnects the speaker first and only then removes amplifier
 *   power. The speaker is therefore never attached while the 36V rail is
 *   rising or collapsing, which is what would otherwise put a thump through
 *   the driver on every schedule transition.
 *
 * POLARITY
 *   cfg::RELAY_ACTIVE_LOW covers opto-isolated boards that energize on a LOW
 *   output. Boot state is de-energized either way: begin() drives both relays
 *   to the inactive level before anything else runs.
 *
 * BLOCKING
 *   wake() and sleep() block for two dwell periods, so roughly one second
 *   each. This is deliberate and accepted: they run only on a schedule
 *   transition or an explicit command, never during playback, and the
 *   sequencing is only meaningful if the settle time is actually observed.
 *   The cost is that the light envelope pauses for that second. If the
 *   watchdog is enabled, the total stays well inside cfg::WDT_TIMEOUT_S.
 */
#ifndef SM_RELAYS_H
#define SM_RELAYS_H

#include <Arduino.h>

class Relays {
public:
  /** Configure both pins as outputs and de-energize, whatever the polarity. */
  void begin();

  /** Amp on, dwell, speaker on. No-op if already awake. */
  void wake();

  /** Speaker off, dwell, amp off. No-op if already asleep. */
  void sleep();

  bool awake() const { return awake_; }

private:
  /** Drive one relay pin, translating through cfg::RELAY_ACTIVE_LOW. */
  void set(uint8_t pin, bool on);

  bool awake_ = false;
};

#endif  // SM_RELAYS_H
