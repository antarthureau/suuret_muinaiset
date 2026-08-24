/**
 * StatusLeds.cpp - implementation of the 4-LED binary status display.
 * See StatusLeds.h for the encoding and why it must not change.
 */
#include "StatusLeds.h"
#include "config.h"

/**
 * Initialize the status LEDs.
 */
void StatusLeds::begin() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(cfg::LED_PINS[i], OUTPUT);
    digitalWrite(cfg::LED_PINS[i], LOW);
  }
}

/**
 * Set the pattern for the status LEDs.
 * @param b3 The state of the third LED.
 * @param b2 The state of the second LED.
 * @param b1 The state of the first LED.
 * @param b0 The state of the fourth LED.
 */
void StatusLeds::pattern(bool b3, bool b2, bool b1, bool b0) {
  const bool v[4] = { b3, b2, b1, b0 };
  for (uint8_t i = 0; i < 4; i++) digitalWrite(cfg::LED_PINS[i], v[i]);
}

/**
 * Show a code on the status LEDs.
 * @param code The code to display.
 */
void StatusLeds::show(int code) {
  if (code < 0 || code > 15) return;

  /*
   * Decompose least-significant bit first into bits[3]..bits[0], so that
   * bits[0] ends up holding bit 3. pattern() then receives them MSB first.
   */
  bool bits[4];
  for (int i = 3; i >= 0; i--) { bits[i] = (code & 1); code >>= 1; }
  pattern(bits[0], bits[1], bits[2], bits[3]);
}
