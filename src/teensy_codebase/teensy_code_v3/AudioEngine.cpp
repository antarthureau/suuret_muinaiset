#include "AudioEngine.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>

void AudioEngine::begin(float volume) {
  // Headroom against SD read-latency spikes. Watch memMax() in the leader's
  // debug line: if it climbs toward this ceiling, playback is starving.
  AudioMemory(120);

  codecOk_ = sgtl_.enable();
  volume_  = volume;
  if (codecOk_) sgtl_.volume(volume_);

  SPI.setMOSI(cfg::SD_MOSI);
  SPI.setSCK(cfg::SD_SCK);
  sdOk_ = SD.begin(cfg::SD_CS);
}

bool AudioEngine::retryInit() {
  if (!codecOk_) { codecOk_ = sgtl_.enable(); if (codecOk_) sgtl_.volume(volume_); }
  if (!sdOk_)    { sdOk_ = SD.begin(cfg::SD_CS); }
  return codecOk_ && sdOk_;
}

bool AudioEngine::play(const char *file) {
  if (!sdOk_) return false;
  wav_.stop();
  return wav_.play(file);
}

void AudioEngine::stop()       { wav_.stop(); }
bool AudioEngine::isPlaying()  { return wav_.isPlaying(); }

void AudioEngine::setVolume(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  volume_ = v;
  if (codecOk_) sgtl_.volume(volume_);
}

float AudioEngine::level(bool peak) {
  if (peak) return peak_.available() ? peak_.read() : -1.0f;
  return rms_.available() ? rms_.read() : -1.0f;
}
