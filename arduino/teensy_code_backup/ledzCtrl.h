/**
 * ledzCtrl.h
 * 
 * This library provides functions to control a 4-LED binary display (4 LEDs array) for displaying status codes (0-15)
 */

#ifndef LEDZCTRL_H
#define LEDZCTRL_H

#include <Arduino.h>

/**
 * Reference to LED_ARRAY defined in the main program
 * Array must contain 4 pin numbers for the status LEDs
 * Pin order: [LED_1, LED_2, LED_3, LED_4]
 */

extern const uint8_t LED_ARRAY[];

/*
* helper function to write on the all four LEDs on the LEDs array
* @valLed1: value for LED_1
* @valLed2: value for LED_2
* @valLed3: value for LED_3
* @valLed4: value for LED_4
*/
void setLedPattern(bool valLed1, bool valLed2, bool valLed3, bool valLed4) {
  bool valuesArray[4] = { valLed1, valLed2, valLed3, valLed4 };

  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_ARRAY[i], valuesArray[i]);
  }
}

/*
 * helper function to display a status code from 0-15 on the 4 LEDs array
 * @code: integer from 0-15 (included)
 */
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

#endif // LEDZCTRL_H