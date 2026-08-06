/**
 * RtcClock.h - DS3231 wrapper, used by the leader only.
 *
 * begin() never overwrites a plausible stored time. It only falls back to
 * compile time when the stored year is clearly invalid (< 2024). Set the
 * time deliberately with setTime() or syncToCompile().
 */
#ifndef SM_RTCCLOCK_H
#define SM_RTCCLOCK_H

#include <Arduino.h>
#include <RTClib.h>

class RtcClock {
public:
  bool begin();                 // false if module not found
  bool ok() const { return ok_; }

  uint8_t hour();
  bool    isActiveHour(uint8_t startHour, uint8_t endHour);

  void setTime(int Y, int Mo, int D, int H, int Mi, int S);
  void syncToCompile();         // last resort; stale by upload gap
  void print(Stream &s);

private:
  RTC_DS3231 rtc_;
  bool ok_ = false;
};

#endif // SM_RTCCLOCK_H
