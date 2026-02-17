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
 */
void setLedPattern(bool valLed1, bool valLed2, bool valLed3, bool valLed4);

/*
 * helper function to display a status code from 0-15 on the 4 LEDs array
 * @code: integer from 0-15 (included)
 */
void displayBinaryCode(int code);

#endif // LEDZCTRL_H
