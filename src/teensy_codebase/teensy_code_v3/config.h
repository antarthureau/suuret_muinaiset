/**
 * config.h - compile-time configuration for Suuret Muinaiset (V3).
 *
 * PURPOSE
 *   Single source of truth for every pin, timing, threshold and protocol
 *   constant in the firmware. Nothing here allocates storage that varies at
 *   runtime: every value is constexpr, so it costs no RAM and may be used
 *   from any translation unit without a definition.
 *
 * WHY ONE FILE
 *   All three units run the same binary. The only thing that differs between
 *   them is which hardware ID strap pin is tied high, so every difference in
 *   behaviour has to be derivable from constants that are identical
 *   everywhere. Keeping them together makes that auditable: if a value is not
 *   in this file, it is not configuration.
 *
 * EDITING RULES
 *   Values here are read at compile time only. Changing one requires a
 *   rebuild and a re-upload to all three units, not just the leader.
 */
#ifndef SM_CONFIG_H
#define SM_CONFIG_H

#include <Arduino.h>

/**
 * Hardware watchdog toggle.
 *
 * With 1, any hang self-recovers after cfg::WDT_TIMEOUT_S. This requires the
 * third-party WDT_T4 library (tonton81) to be installed AND the board target
 * set to a Teensy 4.x. Watchdog_t4.h only declares WDT_T4 and WDOG1 for the
 * i.MX RT chip, so building with a Teensy 3.x selected strips the header to
 * nothing and fails with "'WDOG1' was not declared in this scope".
 *
 * With 0, every watchdog reference compiles out and the library is not
 * needed at all.
 *
 * A hang will NOT self-recover while this is 0. Set it back to 1 before the
 * units are left running unattended on site.
 */
#define USE_WATCHDOG 1

/**
 * Leader audio-health trace toggle.
 *
 * Gates the leader's 1 Hz Serial.printf trace in leaderTask() - the line that
 * reports playback position, peak audio-block usage and peak audio-ISR load
 * while a round is playing.
 *
 * With 0 the whole block compiles out and the USB console stays quiet, which
 * is what you want in normal field operation: the trace otherwise emits a line
 * every second for the entire playing day.
 *
 * Flip to 1 when chasing an audio or CPU problem, then read the output as
 * described in the README. mem climbing toward cfg::AUDIO_MEMORY_BLOCKS, or t
 * failing to advance by roughly 1000 between lines, means the SD path is
 * starving rather than the fault sitting downstream in the analog chain.
 */
#define AUDIO_TRACE 0

namespace cfg {

  /**
   * Hardware ID strap pins.
   *
   * Exactly one of these is tied HIGH on each unit's motherboard; that is
   * what selects the role, the WAV file and the serial address. The pins are
   * read once at boot with the internal pulldown enabled, so an unconnected
   * or broken strap reads LOW and is reported rather than floating to a
   * random role.
   */
  constexpr uint8_t PIN_LONG     = 32;   // leader
  constexpr uint8_t PIN_SMALL    = 30;   // follower 1
  constexpr uint8_t PIN_SEASHELL = 28;   // follower 2

  /**
   * Status LED array, MSB first: LED_PINS[0] is bit 3, LED_PINS[3] is bit 0.
   * The order matches the silkscreen so on-site reading of the array is
   * unchanged from the original build.
   */
  constexpr uint8_t LED_PINS[4]  = { 2, 3, 4, 5 };

  /**
   * Relays.
   *
   * REL_AMP switches mains to the 36V PSU that feeds the amplifier; REL_SPK
   * switches the amplifier output through to the speaker. They are sequenced
   * amp-then-speaker on wake and speaker-then-amp on sleep so the speaker is
   * never connected while the amplifier rail is settling, which is what would
   * otherwise produce a thump through the driver.
   *
   * RELAY_ACTIVE_LOW covers opto-isolated relay boards that energize on a LOW
   * output. Flip it if the relays click in at boot or behave inverted.
   */
  constexpr uint8_t  REL_AMP          = 17;
  constexpr uint8_t  REL_SPK          = 16;
  constexpr bool     RELAY_ACTIVE_LOW = false;
  constexpr uint16_t RELAY_DWELL_MS   = 500;   // settle time between switches

  /**
   * Audio-to-light PWM.
   *
   * The playback envelope drives a MOSFET gate that dims the 12V LED strip on
   * the speaker grille. PWM_FREQ_HZ is deliberately above the audible band:
   * the Teensy 4 default of roughly 4.5 kHz sits squarely in hearing range and
   * couples into the audio path through the shared ground, which is heard as a
   * whine that tracks brightness.
   *
   * PWM_UPDATE_MS sets how often the envelope is resampled. The peak detector
   * produces a new value every 2.9 ms, so 25 ms means each PWM write reflects
   * the peak over the preceding window rather than an instantaneous sample.
   */
  constexpr uint8_t  PWM_PIN        = 6;
  constexpr uint32_t PWM_FREQ_HZ    = 20000;  // above audible
  constexpr uint8_t  PWM_UPDATE_MS  = 25;     // ~40 Hz refresh
  constexpr bool     PEAK_MODE      = true;   // follow peak (true) or RMS

  /**
   * SD card, on the Teensy Audio Shield's socket over SPI.
   *
   * The Teensy 4.0 has no built-in SDIO slot, so SPI is the only transport
   * available here. Playback reads happen inside the audio interrupt, which
   * makes sustained read latency the main risk to clean playback; see
   * AudioEngine for the diagnostics that measure it.
   */
  constexpr uint8_t SD_CS   = 10;
  constexpr uint8_t SD_MOSI = 11;
  constexpr uint8_t SD_SCK  = 13;

  /**
   * Audio buffering, passed to AudioMemory() at startup.
   *
   * The Teensy audio library works in fixed blocks of 128 samples - one block
   * is 2.9 ms of audio at 44.1 kHz - drawn from a single static pool shared by
   * every object in the graph. This value sets how many blocks that pool
   * holds, so it is the ceiling that memMax() is reported against.
   *
   * It is set well above what the graph strictly needs, as headroom against SD
   * read-latency spikes: a card that stalls briefly can be ridden out from the
   * buffer instead of producing a dropout. The Teensy 4 has RAM to spare, so
   * the margin is cheap. If memMax() ever climbs toward this number during
   * playback, the card is not keeping up.
   */
  constexpr uint8_t AUDIO_MEMORY_BLOCKS = 120;

  /**
   * Volume.
   *
   * DEFAULT_VOLUME is applied at every boot; the knob and the serial +/- keys
   * only change the running value and are not persisted. VOL_DEADBAND is the
   * minimum change in the raw pot reading before a new volume is committed,
   * which stops electrical noise on a floating or dirty pot from producing a
   * stream of codec writes and audible zipper noise.
   */
  constexpr uint8_t VOL_POT        = A8;
  constexpr float   DEFAULT_VOLUME = 0.6f;   // headroom; raise carefully
  constexpr float   VOL_DEADBAND   = 0.03f;  // fraction of full scale
  constexpr uint8_t VOL_POLL_MS    = 50;

  /**
   * Daily schedule, evaluated by the leader from its RTC and broadcast to the
   * followers. Active means awake: relays energized and rounds playing.
   *
   * NOTE: technical manual v1.0 documents 07:00-22:00. These values are the
   * ones actually compiled. Reconcile the two before the next site visit.
   */
  constexpr uint8_t START_HOUR = 8;
  constexpr uint8_t END_HOUR   = 20;

  /**
   * Timing.
   *
   * LOOP_GAP_MS is the silence the leader leaves between the end of one round
   * and the broadcast that starts the next. SCHEDULE_POLL_MS is how often the
   * RTC is consulted, HEARTBEAT_MS how often the leader restates awake state
   * so a follower that rebooted mid-cycle resynchronizes on its own.
   *
   * REPORT_POLL_MS and REPORT_STAGGER_MS drive status polling. Followers share
   * one back-channel, so the two are addressed in turn with a gap between
   * them; the gap is timed rather than delayed so the loop keeps servicing the
   * light envelope while it elapses.
   */
  constexpr uint16_t LOOP_GAP_MS        = 500;
  constexpr uint16_t SCHEDULE_POLL_MS   = 30000;
  constexpr uint16_t HEARTBEAT_MS       = 5000;
  constexpr uint32_t REPORT_POLL_MS     = 20000;
  constexpr uint16_t REPORT_STAGGER_MS  = 150;
  constexpr uint32_t LINK_STALE_MS      = 90000;  // follower declares link lost
  constexpr uint8_t  WDT_TIMEOUT_S      = 15;

  /**
   * Recovery from a failed init.
   *
   * A codec or SD card that did not come up at boot is retried in the
   * background. Retries are capped so a permanently absent card cannot keep
   * re-initializing SPI hardware forever, and are suppressed while audio is
   * playing because SD.begin() re-initializes the very card the audio
   * interrupt is streaming from.
   */
  constexpr uint16_t RETRY_INTERVAL_MS = 3000;
  constexpr uint8_t  RETRY_MAX         = 20;

  /**
   * Diagnostics: interval of the leader's audio-health trace while playing.
   * Only compiled in when AUDIO_TRACE is 1.
   */
  constexpr uint16_t DEBUG_TRACE_MS = 1000;

  /**
   * Boot-time fault announcement on the LED array.
   *
   * Faults found during setup - no codec, no SD card, no RTC on the leader -
   * are blinked on the status LEDs at the end of setup(), one code after
   * another, before the unit settles into its normal state display.
   *
   * The blink is what makes them readable: a steady code would be
   * indistinguishable from a normal state at a glance, and simply showing the
   * code once was the old behaviour, where the first pass of loop() overwrote
   * it within microseconds and a dead card read as a healthy sleeping unit.
   *
   * Faults are announced, not latched. A unit that has been running for hours
   * displays its normal state, so to re-read the fault codes on site, reboot
   * the unit - press B on the USB console or power-cycle it - and watch the
   * array during startup.
   *
   * FAULT_BLINK_MS is per fault code, so a unit with all three faults spends
   * three times this long announcing before it starts.
   */
  constexpr uint16_t FAULT_BLINK_MS      = 4000;   // per fault code
  constexpr uint16_t FAULT_BLINK_HALF_MS = 250;    // on/off half-period

  /**
   * Audio files, 8.3 names on the SD card.
   *
   * All three are 16-bit PCM at 44.1 kHz, single channel. Mono is what the
   * units actually run: the masters were re-rendered from the original
   * two-channel files this year, and the deployed set lives in src/audio/mono/.
   * The old 2-channel originals are kept in src/audio/stereo/ for reference
   * only and must not be copied onto a card.
   *
   * That switch is what fixed the CPU spiking and audio glitches in the field.
   * A stereo file costs roughly 176 KB/s off the card; the same content in
   * mono costs 88, and the card is on SPI with no SDIO alternative on a Teensy
   * 4.0. The channel routing in AudioEngine was never the issue - it only ever
   * read output 0 - which is why the fix needed no firmware change at all.
   */
  constexpr char FILE_LONG[]     = "LONG.WAV";
  constexpr char FILE_SMALL[]    = "SMALL.WAV";
  constexpr char FILE_SEASHELL[] = "SEASHELL.WAV";

}  // namespace cfg

/**
 * Player role. Numeric values match the legacy PLAYER_ID from V1/V2 so field
 * notes and status reports stay comparable across firmware versions.
 */
enum class Role : uint8_t { LEADER = 0, SMALL = 1, SEASHELL = 2 };

/**
 * Status codes shown on the 4-LED array as a binary value, 0-15.
 * These are what a technician reads on site with the enclosure open, so the
 * numbering must stay stable across firmware versions.
 */
namespace status {
  constexpr int SLEEP = 1, AWAKE = 2, SD_ERR = 3, CODEC_ERR = 4,
                RTC_ERR = 5, LINK_ERR = 6, REBOOT = 7, PLAYING = 8;
}

/**
 * Serial3 protocol.
 *
 * Frame:  #<addr><cmd><arg>*<XX>\n     XX = 2-hex XOR of addr + cmd + arg
 *
 * One shared half-duplex pair over underground CAT6: the leader's TX reaches
 * both followers, and both followers' TX return on the leader's RX. Only the
 * unit a frame is addressed to may answer, which is what keeps the shared
 * back-channel from colliding without any bus arbitration.
 */
namespace proto {
  constexpr char ADDR_ALL = '0', ADDR_SMALL = '1',
                 ADDR_SEASHELL = '2', ADDR_LEADER = 'L';
  constexpr char CMD_PLAY = 'P', CMD_STOP = 'X', CMD_WAKE = 'W', CMD_SLEEP = 'S';
  constexpr char CMD_HEART = 'H', CMD_REPORT = 'R', CMD_STATUS = 's', CMD_REBOOT = 'B';
  constexpr char CMD_VOL = 'V', CMD_VOLUP = '+', CMD_VOLDN = '-';
  constexpr char CMD_PWMUP = '>', CMD_PWMDN = '<', CMD_KNOB = 'K', CMD_HELP = '?';
}

#endif  // SM_CONFIG_H
