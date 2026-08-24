/**
 * Relays.cpp - implementation of amp/speaker power sequencing.
 * See Relays.h for why the switching order and the dwell time matter.
 */
#include "Relays.h"
#include "config.h"

/**
 * Drive one relay pin, translating through cfg::RELAY_ACTIVE_LOW.
 * @param pin The Teensy pin to drive.
 * @param on  True to energize the relay, false to de-energize.
 */
void Relays::set(uint8_t pin, bool on) {
  const bool level = cfg::RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}
 /**
 * Initialize the relay pins.
 */
void Relays::begin() {
  pinMode(cfg::REL_AMP, OUTPUT);
  pinMode(cfg::REL_SPK, OUTPUT);
  set(cfg::REL_AMP, false);
  set(cfg::REL_SPK, false);
  awake_ = false;
}

/**
 * Wake up the relays.
 */
void Relays::wake() {
  if (awake_) return;
  set(cfg::REL_AMP, true);
  delay(cfg::RELAY_DWELL_MS);   // let the 36V rail come up before connecting
  set(cfg::REL_SPK, true);
  delay(cfg::RELAY_DWELL_MS);   // let the speaker contact settle
  awake_ = true;
}

/**
 * Put the relays to sleep.
 */
void Relays::sleep() {
  if (!awake_) return;
  set(cfg::REL_SPK, false);     // disconnect before the rail collapses
  delay(cfg::RELAY_DWELL_MS);
  set(cfg::REL_AMP, false);
  delay(cfg::RELAY_DWELL_MS);
  awake_ = false;
}
