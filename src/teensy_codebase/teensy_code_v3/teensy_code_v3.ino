/**
 * teensy_code_v3.ino - entry point for the Suuret Muinaiset V3 firmware.
 *
 * ---------------------------------------------------------------------------
 * INSTALLATION
 * ---------------------------------------------------------------------------
 * Three concrete sculptures - LONG, SMALL and SEASHELL - play sound and drive
 * light in response to one another. Each is a sealed IP65 enclosure holding a
 * Teensy 4.0, a Teensy Audio Shield, a dual-channel 150W amplifier, separate
 * 36V (audio) and 12V (control and LED) supplies, an outdoor speaker and an
 * LED strip on the speaker grille.
 *
 * ---------------------------------------------------------------------------
 * ONE BINARY, THREE UNITS
 * ---------------------------------------------------------------------------
 * All three units run this identical firmware. Role is decided at boot by
 * which hardware ID strap pin is tied high, and everything downstream follows
 * from it: which WAV file plays, which serial address the unit answers to,
 * and whether it runs the leader or the follower state machine. There is no
 * per-unit build, so a unit can be swapped in the field without a re-upload.
 *
 * LONG is the leader. It owns the RTC, decides when the installation is awake,
 * and broadcasts the start of every round so the three stay in step. SMALL and
 * SEASHELL are followers: they act on what the leader sends and answer status
 * requests when addressed.
 *
 * ---------------------------------------------------------------------------
 * STRUCTURE
 * ---------------------------------------------------------------------------
 *   config.h        every pin, timing and protocol constant
 *   StatusLeds      4-LED binary status display
 *   Relays          amp and speaker power sequencing
 *   AudioEngine     codec, SD, WAV playback, envelope analysis, diagnostics
 *   RtcClock        DS3231 wrapper (leader only)
 *   SerialLink      addressed, checksummed framing over Serial3
 *   Controller      role detection plus the leader/follower state machines
 *
 * Controller owns one instance of every subsystem. This file creates one
 * Controller and forwards setup() and loop() to it. Nothing else is global,
 * so there is no initialization-order question to reason about beyond the
 * single object below.
 *
 * ---------------------------------------------------------------------------
 * BUILD
 * ---------------------------------------------------------------------------
 * Arduino IDE with Teensyduino. Tools -> Board -> Teensy 4.0, then select the
 * port for the unit being flashed and upload. The board target matters: the
 * watchdog library only declares its types for Teensy 4.x, so a 3.x selection
 * fails to compile even though the library is installed. See USE_WATCHDOG in
 * config.h.
 */
#include "Controller.h"

/** The single Controller instance. Owns every subsystem; see Controller.h. */
Controller controller;

void setup() { controller.setup(); }
void loop()  { controller.loop(); }
