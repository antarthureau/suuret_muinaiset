/**
 * AudioEngine.h - owns the Teensy audio object graph, the codec and the SD card.
 *
 * ---------------------------------------------------------------------------
 * OBJECT GRAPH
 * ---------------------------------------------------------------------------
 *                      +--> out_ ch0  (I2S left)
 *                      |
 *   wav_ (output 0) ---+--> out_ ch1  (I2S right)
 *                      |
 *                      +--> peak_     (envelope for the light)
 *                      |
 *                      +--> rms_      (alternative envelope)
 *
 * The WAV files are 16-bit single-channel mono PCM at 44.1 kHz. Output 0
 * carries that audio, and it is fanned out to both I2S channels so the same
 * signal reaches the amplifier regardless of which side of the minijack is
 * wired, and to both analyzers so the light follows exactly what is heard.
 *
 * The graph has always read output 0 only, so it needed no change when the
 * masters moved from two-channel to true mono - see the note below on why that
 * move mattered anyway.
 *
 * ---------------------------------------------------------------------------
 * WHY THE FILES ARE MONO
 * ---------------------------------------------------------------------------
 * The masters were originally two-channel, and this class read only the left
 * of the pair. That was harmless in itself but not free: samples in a WAV are
 * interleaved L,R,L,R, so the right channel still had to be pulled off the
 * card to reach the left one. A stereo file therefore cost roughly 176 KB/s of
 * SD bandwidth to deliver 88 KB/s of audio.
 *
 * Re-rendering the masters to single-channel mono halved that read rate, and
 * that is what resolved the CPU spiking and audio glitches observed in the
 * field. The fix was entirely in the media, not the firmware.
 *
 * The deployed files live in src/audio/mono/. The old two-channel originals
 * are kept in src/audio/stereo/ for reference and must not go on a card.
 *
 * ---------------------------------------------------------------------------
 * WHERE THE SD READS HAPPEN
 * ---------------------------------------------------------------------------
 * AudioPlaySdWav refills its buffer from inside the audio interrupt, not from
 * loop(). Two consequences shape the rest of this class:
 *
 *   1. A slow main loop does not by itself interrupt playback. Blocking in
 *      loop() stutters the light, not the sound.
 *   2. Anything in the main context that touches the card competes with the
 *      interrupt for it. SD.begin() in particular re-initializes the card the
 *      audio interrupt is streaming from, which is why retryInit() refuses to
 *      run while audio is playing.
 *
 * On a Teensy 4.0 the card is necessarily on the Audio Shield's SPI socket -
 * there is no built-in SDIO slot - so sustained read latency is the realistic
 * failure mode for clean playback. The diagnostics below exist to measure it
 * rather than guess at it.
 *
 * ---------------------------------------------------------------------------
 * INIT IS NON-FATAL
 * ---------------------------------------------------------------------------
 * begin() reports what came up and never blocks. A unit with a failed codec or
 * a missing card still boots, still shows a status code, and still answers on
 * the serial link, so it can be diagnosed remotely instead of appearing dead.
 * retryInit() keeps trying, subject to the interval and cap in config.h.
 */
#ifndef SM_AUDIOENGINE_H
#define SM_AUDIOENGINE_H

#include <Arduino.h>
#include <Audio.h>

class AudioEngine {
public:
  /** Allocate audio memory, enable the codec, mount the card. Never blocks. */
  void begin(float volume);

  bool codecReady() const { return codecOk_; }
  bool sdReady()    const { return sdOk_; }

  /**
   * Attempt to bring up whatever failed at boot. Returns true once both the
   * codec and the card are ready.
   *
   * The caller must not invoke this while audio is playing; see the note above
   * about SD.begin() and the audio interrupt. Controller enforces that.
   */
  bool retryInit();

  /** Start a file from the beginning, replacing anything already playing. */
  bool play(const char *file);
  void stop();
  bool isPlaying();

  /** Clamped to 0.0-1.0. Writes the codec over I2C, so avoid calling it often. */
  void  setVolume(float v);
  float volume() const { return volume_; }

  /** Current envelope 0.0-1.0, or -1.0 if no new sample is available yet. */
  float level(bool peak);

  /* ---------------------------------------------------------------------
   * Diagnostics
   *
   * Together these answer one question: when playback sounds wrong, is the
   * audio engine actually starving, or is it delivering correctly and the
   * fault lies downstream in the analog chain?
   *
   * If memMax() climbs toward the ceiling passed to AudioMemory(), or posMs()
   * stops advancing in real time, the SD path is not keeping up. If both stay
   * healthy through an audible fault, the digital side is fine and the problem
   * is the codec output, the wiring or the amplifier.
   * --------------------------------------------------------------------- */

  // Position within the file currently playing, in milliseconds.
  uint32_t posMs() { return wav_.positionMillis(); }

  // Peak audio blocks in use since the last reset. Ceiling = AudioMemory().
  static uint8_t memMax() { return AudioMemoryUsageMax(); }

  // Peak audio-interrupt load since the last reset, percent.
  static float cpuMax() { return AudioProcessorUsageMax(); }

  // Clear both peaks, so the next reading covers only the next interval.
  static void statsReset() {
    AudioMemoryUsageMaxReset();
    AudioProcessorUsageMaxReset();
  }

private:
  /*
   * Declaration order is load-bearing: members are constructed in the order
   * declared, so the streams must appear before the connections that bind
   * them.
   */
  AudioPlaySdWav       wav_;
  AudioAnalyzePeak     peak_;
  AudioAnalyzeRMS      rms_;
  AudioOutputI2S       out_;
  AudioControlSGTL5000 sgtl_;
  AudioConnection      c1_{ wav_, 0, out_,  0 };   // left -> I2S left
  AudioConnection      c2_{ wav_, 0, out_,  1 };   // left -> I2S right
  AudioConnection      c3_{ wav_, 0, peak_, 0 };   // left -> peak envelope
  AudioConnection      c4_{ wav_, 0, rms_,  0 };   // left -> RMS envelope

  bool  codecOk_    = false;
  bool  sdOk_       = false;
  float volume_     = 0.6f;
  uint8_t retries_  = 0;      // capped by cfg::RETRY_MAX
};

#endif  // SM_AUDIOENGINE_H
