/**
 * StatusLeds.h - 4-LED binary status display.
 *
 * PURPOSE
 *   The only diagnostic available on site without a laptop. A technician opens
 *   the enclosure, reads four LEDs as a binary number, and looks the code up in
 *   the technical manual.
 *
 * ENCODING
 *   Codes 0-15, MSB first. cfg::LED_PINS[0] carries bit 3 and cfg::LED_PINS[3]
 *   carries bit 0, matching the silkscreen order so the reading is the same as
 *   it was on the original V1 build. The code values themselves live in the
 *   status namespace in config.h and must stay stable across firmware
 *   versions, because the printed manual refers to them.
 *
 * BEHAVIOUR
 *   show() is called every loop iteration with the current state rather than
 *   only on change. Writing four digital pins is cheap, and doing it
 *   unconditionally means the display cannot drift out of sync with reality
 *   after a glitch.
 */
#ifndef SM_STATUSLEDS_H
#define SM_STATUSLEDS_H

#include <Arduino.h>

class StatusLeds {
public:
  /** Configure the four pins as outputs and clear the display. */
  void begin();

  /** Display a status code. Values outside 0-15 are ignored. */
  void show(int code);

private:
  /** Write all four LEDs at once, MSB first. */
  void pattern(bool b3, bool b2, bool b1, bool b0);
};

#endif  // SM_STATUSLEDS_H
