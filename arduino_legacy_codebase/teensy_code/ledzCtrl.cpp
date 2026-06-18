/**
 * ledzCtrl.cpp
 *
 * Implementation of LED array control functions.
 */

#include "ledzCtrl.h"

void setLedPattern(bool valLed1, bool valLed2, bool valLed3, bool valLed4) {
  bool valuesArray[4] = { valLed1, valLed2, valLed3, valLed4 };

  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_ARRAY[i], valuesArray[i]);
  }
}

void displayBinaryCode(int code) {
  if (code >= 0 && code <= 15) {
    bool bits[4];

    // Convert the integer to its binary representation (4 bits)
    for (int i = 3; i >= 0; i--) {
      bits[i] = (code & 1);  // Check if the least significant bit is 1
      code >>= 1;            // Right shift to check the next bit
    }

    setLedPattern(bits[0], bits[1], bits[2], bits[3]);
  } else {
    Serial.println("Status code should be an integer in the 0-15 range");
  }
}
