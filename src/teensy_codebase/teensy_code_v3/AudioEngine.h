/**
 * AudioEngine.h - owns the Teensy audio object graph, codec and SD.
 *
 * Mono WAV playback to both I2S channels, with peak and RMS analyzers
 * feeding the light envelope. Init is non-fatal: begin() reports what
 * came up and retryInit() keeps trying to recover codec/SD.
 */
#ifndef SM_AUDIOENGINE_H
#define SM_AUDIOENGINE_H

#include <Arduino.h>
#include <Audio.h>

class AudioEngine {
public:
  void  begin(float volume);    // allocate memory, enable codec, mount SD
  bool  codecReady() const { return codecOk_; }
  bool  sdReady()    const { return sdOk_; }
  bool  retryInit();            // attempt recovery; true when both ready

  bool  play(const char *file);
  void  stop();
  bool  isPlaying();

  void  setVolume(float v);
  float volume() const { return volume_; }

  // Current envelope 0.0-1.0, or -1.0 if no new sample is available.
  float level(bool peak);

  // ---- diagnostics ----
  // Position in the file currently playing, ms.
  uint32_t posMs() { return wav_.positionMillis(); }
  // Peak audio blocks in use since the last reset. The ceiling is the value
  // passed to AudioMemory() in begin(); approaching it means starvation.
  static uint8_t memMax() { return AudioMemoryUsageMax(); }
  // Peak audio-ISR load since the last reset, percent.
  static float   cpuMax() { return AudioProcessorUsageMax(); }
  static void    statsReset() { AudioMemoryUsageMaxReset(); AudioProcessorUsageMaxReset(); }

private:
  AudioPlaySdWav       wav_;
  AudioAnalyzePeak     peak_;
  AudioAnalyzeRMS      rms_;
  AudioOutputI2S       out_;
  AudioControlSGTL5000 sgtl_;
  AudioConnection      c1_{ wav_, 0, out_, 0 };
  AudioConnection      c2_{ wav_, 0, out_, 1 };
  AudioConnection      c3_{ wav_, 0, peak_, 0 };
  AudioConnection      c4_{ wav_, 0, rms_, 0 };

  bool  codecOk_ = false;
  bool  sdOk_    = false;
  float volume_  = 0.6f;
};

#endif // SM_AUDIOENGINE_H
