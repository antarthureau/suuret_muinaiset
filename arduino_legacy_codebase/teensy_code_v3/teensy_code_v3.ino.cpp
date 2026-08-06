/**
 * main.cpp - entry point.
 *
 * The Arduino/Teensy core provides the real main(); it calls setup() once
 * and loop() forever. All logic lives in the Controller and its
 * subsystems, so this file stays trivial.
 *
 * Build:
 *   Open Arduino IDE, select Teensy 4.1 as the board, select which board to upload to, and click Upload.
 */
#include "Controller.h"

Controller controller;

void setup() { controller.setup(); }
void loop()  { controller.loop(); }
