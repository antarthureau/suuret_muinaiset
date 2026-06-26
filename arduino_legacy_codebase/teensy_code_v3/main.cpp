/**
 * main.cpp - entry point.
 *
 * The Arduino/Teensy core provides the real main(); it calls setup() once
 * and loop() forever. All logic lives in the Controller and its
 * subsystems, so this file stays trivial.
 *
 * Build:
 *   PlatformIO  -> builds this file directly (see platformio.ini).
 *   Arduino IDE -> rename this file to <foldername>.ino and keep the other
 *                  .h/.cpp files alongside it in the sketch folder.
 */
#include "Controller.h"

Controller controller;

void setup() { controller.setup(); }
void loop()  { controller.loop(); }
