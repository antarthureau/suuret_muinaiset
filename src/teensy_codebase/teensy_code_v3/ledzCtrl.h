/**
 * ledzCtrl.h
 *
 * 4-LED binary status display (codes 0-15).
 * Pin order in LED_ARRAY is [LED_1, LED_2, LED_3, LED_4], MSB first
 * (LED_1 = bit3, LED_4 = bit0), unchanged from the original so on-site
 * reading of the array stays the same.
 */

#ifndef LEDZCTRL_H
#define LEDZCTRL_H

#include <Arduino.h>

extern const uint8_t LED_ARRAY[];   // 4 status-LED pins, defined in the .ino

/* Write all four LEDs at once. */
inline void setLedPattern(bool b3, bool b2, bool b1, bool b0) {
  const bool v[4] = { b3, b2, b1, b0 };
  for (int i = 0; i < 4; i++) digitalWrite(LED_ARRAY[i], v[i]);
}

/* Display a status code 0-15 on the array. Out-of-range is ignored
   (no Serial dependency here, so this header stays self-contained). */
inline void displayBinaryCode(int code) {
  if (code < 0 || code > 15) return;
  bool bits[4];
  for (int i = 3; i >= 0; i--) { bits[i] = (code & 1); code >>= 1; }
  setLedPattern(bits[0], bits[1], bits[2], bits[3]);
}

#endif // LEDZCTRL_H
