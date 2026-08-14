/**
 * AudioEngine.cpp - codec, SD and WAV playback implementation.
 * See AudioEngine.h for the object graph and why SD reads happen in interrupt
 * context.
 */
#include "AudioEngine.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>

/**
 * Allocate audio memory, enable the codec, mount the card. Never blocks.
 * @param volume The initial codec volume, 0.0-1.0.
 */
void AudioEngine::begin(float volume) {
  /*
   * Headroom against SD read-latency spikes. See cfg::AUDIO_MEMORY_BLOCKS for
   * what a block is and why the pool is deliberately oversized. Watch memMax()
   * in the leader's trace: if it climbs toward this ceiling during playback,
   * the card is not keeping up.
   */
  AudioMemory(cfg::AUDIO_MEMORY_BLOCKS);

  codecOk_ = sgtl_.enable();
  volume_  = volume;
  if (codecOk_) sgtl_.volume(volume_);

  /*
   * SPI pins must be selected before SD.begin(), which latches them. MISO is
   * left at the Teensy 4 default (pin 12), which is what the Audio Shield uses.
   */
  SPI.setMOSI(cfg::SD_MOSI);
  SPI.setSCK(cfg::SD_SCK);
  sdOk_ = SD.begin(cfg::SD_CS);
}

/**
 * Attempt to bring up whatever failed at boot. Returns true once both the
 * codec and the card are ready.
 * @return True if both the codec and the SD card are ready, false otherwise.
 */
bool AudioEngine::retryInit() {
  /*
   * Caller is responsible for not invoking this during playback: SD.begin()
   * re-initializes the card that the audio interrupt is reading from. The cap
   * stops a permanently absent card from re-initializing SPI forever.
   */
  if (retries_ >= cfg::RETRY_MAX) return codecOk_ && sdOk_;
  retries_++;

  if (!codecOk_) {
    codecOk_ = sgtl_.enable();
    if (codecOk_) sgtl_.volume(volume_);
  }
  if (!sdOk_) {
    sdOk_ = SD.begin(cfg::SD_CS);
  }
  return codecOk_ && sdOk_;
}

/**
 * Function to play an audio file from the SD card.
 * @param file The name of the audio file to play.
 * @return True if the file was successfully started, false otherwise.
 */
bool AudioEngine::play(const char *file) {
  if (!sdOk_) return false;
  wav_.stop();                  // rewind if a round is somehow still running
  return wav_.play(file);
}

/**
 * Function to stop the audio playback.
 */
void AudioEngine::stop()      { wav_.stop(); }

/**
 * Function to check if the audio is playing.
 * @return True if audio is playing, false otherwise.
 */
bool AudioEngine::isPlaying() { return wav_.isPlaying(); }

/**
  * Function to set the volume of the codec.
  * The codec is written over I2C, so avoid calling this function often.
  * @param v The volume to set, clamped to 0.0-1.0.
 */
void AudioEngine::setVolume(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  volume_ = v;
  if (codecOk_) sgtl_.volume(volume_);
}

/**
 * Function to get the audio level.
 * @param peak True to get the peak level, false to get the RMS level.
 * @return The audio level, or -1.0 if no new data is available.
 */
float AudioEngine::level(bool peak) {
  /*
   * Both analyzers run, but only the selected one is read. available() is
   * false until the analyzer has accumulated a new window, so -1.0 means
   * "nothing new yet" rather than "silence" - the caller must not treat it as
   * a level.
   */
  if (peak) return peak_.available() ? peak_.read() : -1.0f;
  return rms_.available() ? rms_.read() : -1.0f;
}
