/**
 * RtcClock.h - DS3231 real-time clock wrapper. Leader only.
 *
 * PURPOSE
 *   The leader is the only unit that knows what time it is. It consults this
 *   clock periodically, decides whether the installation should be awake, and
 *   broadcasts that decision. Followers hold no schedule of their own; they do
 *   what the leader's wake/sleep commands and heartbeat tell them, which means
 *   there is exactly one clock to set and one to go wrong.
 *
 * WHY THE TIME IS NOT OVERWRITTEN AT BOOT
 *   Earlier firmware wrote compile time into the RTC on every startup. That
 *   silently destroyed a correct time whenever anyone re-flashed a unit, and
 *   it made a dying coin cell indistinguishable from normal operation.
 *
 *   begin() now only falls back to compile time when the stored year is
 *   clearly impossible (before 2026), and reports the fallback instead of
 *   hiding it. Otherwise the stored time is kept, even after a lostPower()
 *   flag, and the operator sets the clock deliberately with the :settime or
 *   :synctime console commands.
 *
 *   A unit that keeps reporting lost power on every boot has a dead or missing
 *   DS3231 coin cell. Replace it rather than working around it: without a
 *   backup cell the schedule resets to compile time at every power cycle,
 *   which on an installation that sleeps overnight means it never wakes when
 *   it should.
 *
 * FAILURE BEHAVIOUR
 *   A missing module is not fatal. ok() stays false, isActiveHour() returns
 *   false, and the leader shows status::RTC_ERR on the LED array. The
 *   installation stays asleep rather than running at an unknown hour.
 */
#ifndef SM_RTCCLOCK_H
#define SM_RTCCLOCK_H

#include <Arduino.h>
#include <RTClib.h>

class RtcClock {
public:
  /** Probe the module. False if it is not on the bus. */
  bool begin();

  bool ok() const { return ok_; }

  /** Current hour 0-23, or 0 if the module is unavailable. */
  uint8_t hour();

  /**
   * True when the current hour is within [startHour, endHour).
   * Always false if the module is unavailable, so a failed RTC keeps the
   * installation asleep rather than running at an unknown time.
   */
  bool isActiveHour(uint8_t startHour, uint8_t endHour);

  /** Set the clock explicitly. This is the intended way to set the time. */
  void setTime(int Y, int Mo, int D, int H, int Mi, int S);

  /**
   * Set the clock to this build's compile time. Convenient but always stale
   * by however long elapsed between compiling and uploading, so prefer
   * setTime() when accuracy matters.
   */
  void syncToCompile();

  /** Print the current time to a stream, or a notice if unavailable. */
  void print(Stream &s);

private:
  RTC_DS3231 rtc_;
  bool ok_ = false;
};

#endif  // SM_RTCCLOCK_H
