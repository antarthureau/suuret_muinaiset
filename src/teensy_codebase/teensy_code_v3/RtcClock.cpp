#include "RtcClock.h"

bool RtcClock::begin() {
  ok_ = rtc_.begin();
  if (!ok_) return false;

  if (rtc_.lostPower()) {
    DateTime n = rtc_.now();
    if (n.year() < 2024) {
      rtc_.adjust(DateTime(F(__DATE__), F(__TIME__)));   // last-resort fallback
    }
    // else: backup time looks plausible, keep it. A repeated lostPower()
    // means the DS3231 coin cell is dead or missing - replace it.
  }
  return true;
}

uint8_t RtcClock::hour() {
  return ok_ ? rtc_.now().hour() : 0;
}

bool RtcClock::isActiveHour(uint8_t startHour, uint8_t endHour) {
  if (!ok_) return false;
  uint8_t h = rtc_.now().hour();
  return (h >= startHour && h < endHour);
}

void RtcClock::setTime(int Y, int Mo, int D, int H, int Mi, int S) {
  if (ok_) rtc_.adjust(DateTime(Y, Mo, D, H, Mi, S));
}

void RtcClock::syncToCompile() {
  if (ok_) rtc_.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void RtcClock::print(Stream &s) {
  if (!ok_) { s.println("RTC unavailable"); return; }
  DateTime t = rtc_.now();
  char buf[32];
  snprintf(buf, sizeof(buf), "%04u/%02u/%02u %02u:%02u:%02u",
           t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
  s.println(buf);
}
