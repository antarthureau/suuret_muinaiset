/**
 * Relays.h - amp/speaker power sequencing.
 *
 * Boot state is always de-energized regardless of board polarity. wake()
 * powers amp then speaker; sleep() powers speaker off then amp, each with
 * a settle delay. Switching is a no-op if already in the requested state.
 */
#ifndef SM_RELAYS_H
#define SM_RELAYS_H

#include <Arduino.h>

class Relays {
public:
  void begin();                 // configure pins, de-energize
  void wake();                  // amp -> speaker
  void sleep();                 // speaker -> amp
  bool awake() const { return awake_; }

private:
  void set(uint8_t pin, bool on);
  bool awake_ = false;
};

#endif // SM_RELAYS_H
