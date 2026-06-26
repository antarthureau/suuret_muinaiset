/**
 * StatusLeds.h - 4-LED binary status display (codes 0-15).
 */
#ifndef SM_STATUSLEDS_H
#define SM_STATUSLEDS_H

#include <Arduino.h>

class StatusLeds {
public:
  void begin();
  void show(int code);          // 0-15; out-of-range ignored

private:
  void pattern(bool b3, bool b2, bool b1, bool b0);
};

#endif // SM_STATUSLEDS_H
