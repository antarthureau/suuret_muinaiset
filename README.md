# suuret_muinaiset system (V3)

## Overview

The installation consists of three Teensy 4.0 microcontrollers, one per creature (LONG, SMALL, SEASHELL), each independently playing its own audio file, driving its own amplifier/speaker, and computing its own LED brightness from real time audio feed.

The firmware in `src/teensy_codebase/teensy_code_v3` is a rewrite of the original Teensy codebase (`teensy_code_v1` then `teensy_code_v2`, kept in the repo for reference): same wiring and connectors, but a non-blocking, watchdog-protected, checksummed-serial implementation.

LONG (green) is the leader and carries the only RTC. SMALL (yellow) and SEASHELL (red) are followers. All three units run the identical compiled firmware; a unit's role is decided at boot by which hardware ID strap pin reads HIGH, not by a build flag.

The single Raspberry Pi 5 / Pure Data approach (V2) has been dropped for now, but its codebase is available in src.

## Unit hardware

Each of the three units is an IP65 enclosure containing:

Teensy 4.0 microcontroller on a custom motherboard that breaks out power, relays, the LED PWM output, Serial3, the volume potentiometer input (unused) and the 4-LED status array.

Official Teensy Audio Shield (SGTL5000 codec), with a micro SD card slot underneath the Teensy board holding that unit's WAV file.

Dual-channel 150 W audio amplifier (WONDOM AA-AB32221): Only one channel is used per unit; if it fails, rewire to the spare channel before replacing the whole amplifier board.

36V DC PSU (LOP-300-36): powers the amplifier, switched by Relay 2.
12V DC PSU (PSK-15D-12-T): powers the Teensy, its motherboard, and LED strip, always on. During sleep only the 12V DC PSU indicator stays lit; during wake both PSU LEDs are lit.

4-relay board (Velleman WPM400): only two relays wired; Relay 1 switches the amplifier output through to the speaker, Relay 2 switches the 220V AC feed to the 36V DC PSU. If one relay fails, the two spares on the same board can be re-cabled in as replacements; if all four fail, replace the board (Velleman ).

DS3231 RTC module - LONG (leader) only, over I2C.

Outdoor-rated speaker (Dayton WP8BT) and an LED strip mounted on the speaker grille, driven by an IRL520 MOSFET on the motherboard, switched by the Teensy's PWM output.

Enclosure connectors: USB-A (upload/serial console), EtherCON (Serial3, underground CAT6 to the other units), a second EtherCON (PWM/LED output + volume pot on LONG), PowerCON (amplifier-to-speaker output), plus the 230V AC mains input distributed locally via Wago connectors. A pressure-compensating vent and a silica gel tank keep humidity and pressure stable inside each box.

## Site wiring

LONG, SMALL and SEASHELL each sit in their own box on site, linked back to a mains enclosure by underground CAT6 (a custom Y-cable splits the serial run to both followers from the leader) and underground power lines. Each box also carries a regular CAT6 run out to its own speaker/light EtherCON connector. 230V AC arrives at the mains enclosure and is distributed to each unit.

## Audio signal path

The Teensy plays its unit's WAV file from the SD card (`cfg::FILE_LONG/SMALL/SEASHELL` in `config.h`, 8.3 names `LONG.WAV`/`SMALL.WAV`/`SEASHELL.WAV`; the matching source files live in `src/audio` as `LONG.wav`/`SMALL.wav`/`SEASHELL.wav` - case doesn't matter on FAT. `TESTLOOP.wav` is also in that folder for bench testing) through the Teensy Audio Shield's SGTL5000 codec. The Audio Shield outputs on a 3.5mm TRS minijack, which feeds an RCA input on the amplifier board. The amplifier's output goes through a terminal block to Relay 1, which (when awake) connects it to a PowerCON chassis socket (NAC3FPX-TRUE1, mating plug NAC3M-TRUE1-L) and out to the speaker.

## Light signal path

`AudioAnalyzePeak` and `AudioAnalyzeRMS` objects are tapped off the same WAV playback stream inside the Teensy's audio object graph (see `AudioEngine`). Every ~25 ms (`cfg::PWM_UPDATE_MS`), the controller reads the current peak or RMS level (`cfg::PEAK_MODE` selects which), scales it by a configurable range (`rangePwm_`, adjustable at runtime with `>`/`<`), and writes it to the Teensy's 8-bit PWM output (pin 6). That PWM signal runs over CAT6 through the second EtherCON connector to the LED daughter board, whose IRL520 MOSFET switches the 12V LED strip current in proportion to the signal - functionally the same envelope-to-light behavior as the old analog PCB, just computed in firmware instead of with op-amps.

## Firmware / codebase

Active firmware lives in `src/teensy_codebase/teensy_code_v3`:

```
config.h            compile-time pins, timings, status codes, protocol constants
StatusLeds.h/.cpp   4-LED binary status display
Relays.h/.cpp       amp/PSU + speaker relay sequencing
AudioEngine.h/.cpp  SGTL5000 codec, SD WAV playback, peak/RMS analyzers
RtcClock.h/.cpp     DS3231 wrapper (leader only)
SerialLink.h/.cpp   addressed, checksummed Serial3 framing
Controller.h/.cpp   role detection + leader/follower state machine
teensy_code_v3.ino.cpp   entry point (setup/loop), builds one Controller
```

`Controller` owns one instance of every subsystem and is the only global object. `ledzCtrl.h` and `sysCtrl.h` also sit in this folder but are dead code from an earlier procedural pass - the real entry point only includes `Controller.h`, so they're not part of the running firmware. `teensy_code_v1` and `teensy_code_v2` are earlier iterations of the same design, kept for reference; `RTC_setup_code/RTC_setup_code.ino` is a standalone sketch (originally for an Arduino Uno) for bench-testing or reprogramming a DS3231 module off the installation.

To build: open the `teensy_code_v3` folder in the Arduino IDE with Teensyduino installed, select the Teensy board that matches the physical MCU, select the target unit's port, and upload. The hardware is Teensy 4.0 (per the manual and every wiring reference); `teensy_code_v3.ino.cpp`'s own build comment says "Teensy 4.1" - that comment is stale, select 4.0 to match the physical board. All three units take the same compiled sketch - role comes from the strap pins, not from a build-time setting.

Role is detected once at boot in `Controller::detectRole()` by reading three digital pins: pin 32 HIGH selects LEADER (LONG), pin 30 HIGH selects SMALL, pin 28 HIGH selects SEASHELL; if none read HIGH the unit defaults to LEADER. A hardware watchdog (`Watchdog_t4`, 15 s timeout, `USE_WATCHDOG` in `config.h`) resets the unit if the main loop ever hangs; disable it only if the `Watchdog_t4` library isn't installed.

## Teensy pinout

| Pin | Function |
|---|---|
| 0, 1 | Reserved for USB Serial - must be left unconnected |
| 2-5 | Status LEDs 1-4 (binary array, LED 1 = MSB) |
| 6 | PWM output to LED strip driver |
| 7, 8 | Unused (Serial2-capable) |
| 9, 12 | Unused |
| 10 | SD card CS |
| 11 | SD card MOSI |
| 13 | SD card SCK |
| 14 | Serial3 RX - receive (SMALL/SEASHELL only) |
| 15 | Serial3 TX - transmit (LONG only) |
| 16 | Relay 1 - speaker switching |
| 17 | Relay 2 - 36V PSU / amp power switching |
| 18 | RTC SDA (I2C, LONG only) |
| 19 | RTC SCL (I2C, LONG only) |
| 20 | Audio Shield LRCLK |
| 21 | Audio Shield BCLK |
| 22 / A8 | Volume potentiometer analog input |
| 23 | Audio Shield MCLK |
| 28 | Hardware ID strap - SEASHELL |
| 30 | Hardware ID strap - SMALL |
| 32 | Hardware ID strap - LONG (leader) |

Serial3 and the PWM/volume line run out through EtherCON chassis sockets (NE8FDPU-TOP) over CAT6, wired as follows:

The serial link's RJ45 pinout is mirrored between the two ends of the run so that the leader's Tx lands on the follower's Rx: one end is wired 1 Rx / 2 GND / 3 Tx, the other 1 Tx / 2 GND / 3 Rx (pins 4-8 unused on both). See `documentation/manual/manual v1.0.pdf` page 8 for the exact per-unit wiring diagram before re-terminating a connector.

PWM/volume RJ45 (LONG, carries the pot signal in addition to PWM): 1 NC, 2 GND, 3 NC, 4 +3.3V, 5 A8/pot signal, 6 NC, 7 PWM+, 8 PWM-.
PWM RJ45 (SMALL/SEASHELL): 1-6 NC, 7 PWM+, 8 PWM-.

## Serial3 link protocol

The three units share one half-duplex Serial3 bus at 9600 baud (leader TX -> both followers' RX, followers' TX -> leader RX). Every frame is addressed and checksummed so only the intended recipient(s) ever act on it and a garbled frame is dropped rather than misread:

```
#<addr><cmd><arg>*<XX>\n
```

`addr` is `0` (broadcast to both followers), `1` (SMALL), `2` (SEASHELL) or `L` (leader - used only for status replies). `cmd` is a single command character, `arg` an optional short ASCII payload. `XX` is the 2-hex-digit XOR checksum of everything between `#` and `*`. `SerialLink::poll()` builds one frame at a time without blocking; overflow, a missing `*`, or a checksum mismatch just drops that frame and keeps listening.

Command characters (shared with the USB console below):

| Char | Command | Notes |
|---|---|---|
| `P` | Play | Leader broadcasts this once per playback round so all three stay in sync |
| `X` | Stop | Stops playback, zeroes the PWM output |
| `W` | Wake | Runs the relay wake sequence (36V PSU on, then speaker on) |
| `S` | Sleep | Runs the relay sleep sequence (speaker off, then 36V PSU off) |
| `H` | Heartbeat | Leader -> followers every 5 s, `arg` = `1`/`0` current awake state; a follower resyncs its own awake state from this and resets its link-timeout timer |
| `R` | Report request | Leader polls each follower in turn; the follower replies once with `addr=L, cmd=s` |
| `s` | Status reply | Follower -> leader only, `arg` = `role,awake,playing,cpu_temp` |
| `B` | Reboot | Leader broadcasts, then every unit resets itself after a 3 s watchdog-fed delay |
| `V` | Set volume | `arg` = float 0.0-1.0 |
| `+` / `-` | Volume up/down | Steps by 0.1 |
| `>` / `<` | PWM range up/down | Steps the light-output ceiling by 25 (0-255) |
| `K` | Toggle knob control | Enables/disables the physical volume pot overriding software volume |
| `?` | Help | Prints the command summary to USB serial (not relayed over Serial3) |

A follower is considered link-stale (status LED shows LINK_ERR) if it hasn't heard a heartbeat in 90 s - almost always a Serial3/CAT6 cabling problem rather than a leader crash, since the leader's watchdog would otherwise reboot it long before that.

## USB serial console

Connect USB to any unit (closed or open enclosure) and open a serial terminal at 9600 baud - Arduino IDE's Serial Monitor, PuTTY, `screen`, etc. This is the primary way to talk to a unit directly, independent of the Serial3 link.

Single-key commands: `P` play, `X` stop, `W` wake, `S` sleep, `R` report, `B` reboot, `+`/`-` volume, `>`/`<` PWM range, `K` toggle knob, `?` help. On the leader, `P X W S + - > < B` are also broadcast to the followers over Serial3 before being applied locally; `R`, `K` and `?` are local-only. On a follower, any key only affects that follower.

Line commands (start with `:`, terminated by Enter or a 200 ms pause):

`:settime YYYY MM DD HH MM SS` - sets the RTC directly. Only meaningful on the leader; a follower's `RtcClock` was never initialized, so the call is silently a no-op there.
`:synctime` - sets the RTC to the sketch's compile time. Last resort only - it will be stale by however long ago the firmware was built.
`:time` - prints the current RTC time.

`R` (report) prints role, current WAV file, RTC time (leader only), awake/playing state, volume, PWM range, playback round count (since the last wake), and CPU temperature - the fastest way to see a unit's full state at once.

## Status LED codes

The 4-LED binary array on each motherboard displays a 0-15 status code (LED 1 = MSB, LED 4 = LSB):

| Code | Binary | Meaning |
|---|---|---|
| 1 | 0001 | Sleep - normal, awaiting wake |
| 2 | 0010 | Awake, idle (between playback rounds) |
| 3 | 0011 | SD card not found / not readable |
| 4 | 0100 | SGTL5000 codec (Audio Shield) not found |
| 5 | 0101 | RTC not found (leader only) |
| 6 | 0110 | Serial3 link stale - no heartbeat for 90 s (followers only) |
| 7 | 0111 | Rebooting |
| 8 | 1000 | Playing |

Binary 1 (0001) during the sleep window and periodic sound/light every playback loop are the normal-operation baseline described in the manual.

## Daily schedule

The leader checks the RTC every 30 s and broadcasts `W`/`S` (plus its own heartbeat state) whenever the awake window changes. The current firmware default is `cfg::START_HOUR = 8`, `cfg::END_HOUR = 20` in `config.h` (awake 08:00-20:00, asleep 20:00-08:00); the printed manual describes the original 07:00-22:00 window. To change the schedule, edit `START_HOUR`/`END_HOUR` in `config.h` and re-upload to the leader only (followers just take their awake state from the leader's heartbeat, they don't read the RTC themselves). Unlike the Pi's full power-off cycle, the Teensy units stay powered through sleep; only the 36V amp PSU and speaker relay drop out, so the MCU, watchdog and Serial3 link keep running around the clock.

## Debugging and troubleshooting

Start with the status LED code on the affected unit, then confirm with the USB console (`R` for a full report, `?` for the command list).

SD_ERR (3): confirm the card is seated (slot is underneath the Teensy board), formatted FAT32, and contains the exact 8.3 filename that unit expects (`LONG.WAV` / `SMALL.WAV` / `SEASHELL.WAV`). The firmware retries codec/SD init every 3 s on its own (`AudioEngine::retryInit`), so a card reseated live should recover without a reboot.

CODEC_ERR (4): the Teensy Audio Shield isn't responding - reseat it on the header, check its solder joints and 3.3V supply.

RTC_ERR (5, leader only): DS3231 not detected on I2C (pins 18/19) - check wiring first, then the module itself. If the RTC repeatedly reports lost power (visible over USB serial) even when wiring is fine, the coin cell is dead and needs replacing.

LINK_ERR (6, followers only): no heartbeat in 90 s. Check the Serial3 CAT6 run and its EtherCON connectors (RJ45 pin 1/3 Tx/Rx swap between leader and follower ends, pin 2 GND) before suspecting the leader itself, since the leader's own watchdog would otherwise have already recovered it.

No sound / no light on one unit: verify both PSU indicator LEDs are lit while awake (36V and 12V), check amplifier RCA input and minijack from the Audio Shield, and confirm the correct WAV file is present. If the amplifier's active channel has failed, the amp has a second channel that can be rewired to instead of replacing the board outright.

Relay problems: only 2 of the 4 relays on the relay board are wired in (Relay 1 = speaker, Relay 2 = 36V PSU). A single failed relay can be swapped for one of the two spares on the same board by re-cabling; total failure means replacing the board.

Unit unresponsive on USB but LEDs still cycling: the watchdog (15 s trigger) will self-recover any firmware hang; if a unit stays hard-locked longer than that, suspect a hardware fault rather than firmware, since the loop is intentionally non-blocking end to end.

Malformed or missing Serial3 traffic: frames with a bad checksum, a stray `#` mid-frame, or overflow past 64 bytes are dropped silently by design - a technician who sees no follower replies to `R` on the leader's USB console should check cable continuity on the long underground CAT6 run first.

## Required toolkit

Flat screwdriver 3mm for terminal blocks, flat screwdriver 6mm and Phillips 6mm for enclosure fasteners and general use.

Multimeter for voltage and continuity testing (230V AC, 12V DC, 36V DC).

Phillips 000/00 and hex keys 4mm/4.5mm for board- and module-level service, plus an anti-static wrist strap.

Wago connectors and crocodile clips for temporary test connections.

3.5mm stereo headphones for testing audio directly off the Teensy Audio Shield.

A computer with a USB-A port, the Arduino IDE with Teensyduino installed, and a serial monitor (Arduino Serial Monitor, PuTTY, etc.).

For workshop-level diagnostics: an Arduino Uno R3 for RTC module reprogramming/testing (see `RTC_setup_code`), an oscilloscope for signal analysis, a spare Teensy 4.0 for component-swap testing, and a bench power supply for isolated testing.

A long CAT6 test cable or cable tester for diagnosing the underground serial/PWM runs between units.

## Warranty and support

Warranty period: 04.06.2025 - 04.06.2026. Support contact during the warranty period: ahureau@pm.me (designer) or the on-site support technician in Turku. Typical response time 24-48 hours.

## References

`documentation/manual/manual v1.0.pdf` - full technical manual with block diagrams, connector pinout charts, and the physical wiring reference this README is derived from.
`documentation/editable doc.drawio` / `editable doc v2.drawio` - editable source diagrams.
`documentation/3D prints stl files/` - enclosure gaskets and mounting parts.

Note: `documentation/schematics v2.pdf` documents the analog envelope-follower PCB used in the previous Pi-based (V2) system and does not apply to this Teensy build - the Teensy computes the light envelope in firmware instead.
