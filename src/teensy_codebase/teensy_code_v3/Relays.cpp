/**
 * Relays.cpp - implementation of amp/speaker power sequencing.
 * See Relays.h for why the switching order and the dwell time matter.
 */
#include "Relays.h"
#include "config.h"

void Relays::set(uint8_t pin, bool on) {
  const bool level = cfg::RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

void Relays::begin() {
  pinMode(cfg::REL_AMP, OUTPUT);
  pinMode(cfg::REL_SPK, OUTPUT);
  set(cfg::REL_AMP, false);
  set(cfg::REL_SPK, false);
  awake_ = false;
}

void Relays::wake() {
  if (awake_) return;
  set(cfg::REL_AMP, true);
  delay(cfg::RELAY_DWELL_MS);   // let the 36V rail come up before connecting
  set(cfg::REL_SPK, true);
  delay(cfg::RELAY_DWELL_MS);   // let the speaker contact settle
  awake_ = true;
}

void Relays::sleep() {
  if (!awake_) return;
  set(cfg::REL_SPK, false);     // disconnect before the rail collapses
  delay(cfg::RELAY_DWELL_MS);
  set(cfg::REL_AMP, false);
  delay(cfg::RELAY_DWELL_MS);
  awake_ = false;
}
