# Suuret Muinaiset system (V3)

## Overview

The installation has three units. Each unit is one creature: LONG, SMALL or
SEASHELL. Each unit has a Teensy 4.0 microcontroller.

Each unit plays its own audio file. It drives its own amplifier and speaker. It
also calculates its own LED brightness from the audio signal.

LONG is the leader. It is green, and it has the only RTC. SMALL (yellow) and
SEASHELL (red) are followers.

All three units use the same compiled firmware. At start-up, each unit reads
three hardware ID strap pins. The pin that reads HIGH selects the role.
Therefore the role is not a build option.

The firmware is in `src/teensy_codebase/teensy_code_v3`.

`teensy_code_v1` and `teensy_code_v2` stay in the repository for reference. The
Raspberry Pi 5 and Pure Data system is not in use. Its code stays in `src`.

## Unit hardware

Each unit is an IP65 enclosure. It contains the equipment that follows.

- Teensy 4.0 microcontroller on a custom motherboard. The motherboard connects
  the power, the relays, the LED PWM output, Serial3, the volume potentiometer
  input and the 4-LED status array.
- Teensy Audio Shield with an SGTL5000 codec. The micro SD card slot is below
  the Teensy board. The card holds the WAV file for that unit.
- Dual-channel 150 W audio amplifier (WONDOM AA-AB32221). Each unit uses one
  channel only. If the channel becomes defective, connect the spare channel.
  Do not replace the board immediately.
- 36V DC PSU (LOP-300-36). It supplies the amplifier. Relay 2 switches it.
- 12V DC PSU (PSK-15D-12-T). It supplies the Teensy, the motherboard and the
  LED strip. It is always on.
- 4-relay board (Velleman WPM400). Two relays are in use. Relay 1 connects the
  amplifier output to the speaker. Relay 2 connects the 220V AC supply to the
  36V DC PSU. Two spare relays are on the same board.
- DS3231 RTC module on I2C. LONG only.
- Outdoor speaker (Dayton WP8BT).
- LED strip on the speaker grille. A MOSFET on the motherboard drives it.
  The PWM output of the Teensy controls the MOSFET.

During sleep, only the 12V DC PSU indicator is on.

The enclosure has these connectors:

- USB-A for the upload and the serial console.
- EtherCON for Serial3, on underground CAT6 to the other units.
- A second EtherCON for the PWM and LED output. On LONG it also carries the
  volume potentiometer signal.
- PowerCON for the amplifier output to the speaker.
- 230V AC mains input. Wago connectors distribute it in the enclosure.

A pressure-compensating vent and a silica gel tank keep the humidity and the
pressure stable in each enclosure.

## Site wiring

Each unit is in its own enclosure on site. Underground CAT6 connect the three enclosures.
A custom Y-cable divides the serial run from the leader etherCON to the underground CAT6 lines
connecting to the follower units etherCON.

Each unit gets a 230V AC supply comes to its enclosure.

## Audio signal path

The Teensy reads the WAV file for its unit from the SD card. `config.h` gives
the file names in `cfg::FILE_LONG`, `cfg::FILE_SMALL` and `cfg::FILE_SEASHELL`.
The names on the card are `LONG.WAV`, `SMALL.WAV` and `SEASHELL.WAV`. A FAT
file system ignores the letter case.

The source files are in `src/audio/mono/`. `TESTLOOP.WAV` is also in that
folder for bench tests.

The SGTL5000 codec on the Audio Shield converts the audio. The Audio Shield has
a 3.5 mm TRS minijack output. This output goes to an RCA input on the amplifier
board.

The amplifier output goes through a terminal block to Relay 1. When the unit is
awake, Relay 1 connects the output to a PowerCON socket (NAC3FPX-TRUE1). The
mating plug on the speaker cable outside the enclosure is NAC3M-TRUE1-L.

### Why the audio files have one channel

The WAV files are 16-bit PCM at 44.1 kHz. They have one channel (mono).

The first masters had two channels. The firmware read only channel 0 from them.
This did no damage. But it used more bandwidth than necessary.

The samples in a WAV file are in this sequence: L, R, L, R. Therefore the unit
must read the full file from the SD card to get channel 0. A file with two
channels used 176 KB/s of SD card bandwidth. But it gave only 88 KB/s of audio.

New masters with one channel decreased the necessary bandwidth by 50 percent.
This change corrected a high CPU load and audio noise on site. The correction
was in the audio files, not in the firmware.

The first files with two channels are in `src/audio/stereo/`. Keep them for
reference only. Do not copy them to an SD card.

`src/audio/make_mono.sh` makes the one-channel files again from the two-channel
files. It keeps channel 0 without a change to the samples.

### SD card and interrupts

`AudioPlaySdWav` reads the SD card in the audio interrupt. It does not read the
card in `loop()`. Two results are important:

- A slow main loop does not stop the audio. It makes the light irregular.
- Other code that uses the SD card can cause a conflict with the interrupt.

`AudioEngine::retryInit()` therefore does not operate while audio plays.
`SD.begin()` starts the same card that the interrupt reads.

The Teensy 4.0 has no internal SDIO socket. Therefore the card must be in the
socket of the Audio Shield, on the SPI bus. A slow SD card is the most probable
cause of an audio failure.

## Light signal path

`AudioAnalyzePeak` and `AudioAnalyzeRMS` objects connect to the same playback
stream in the audio object graph. Refer to `AudioEngine`.

Each 25 milliseconds (`cfg::PWM_UPDATE_MS`), the controller reads the current
level. `cfg::PEAK_MODE` selects the peak value or the RMS value. The controller
multiplies the level by a range value (`rangePwm_`). The keys `>` and `<`
change this range during operation. Then the controller writes the result to
the 8-bit PWM output on pin 6.

The PWM frequency is 20 kHz (`cfg::PWM_FREQ_HZ`). This frequency is above the
audible range. The default frequency of the Teensy 4 is approximately 4.5 kHz.
That frequency is audible, and it goes into the audio circuit through the
common ground.

The PWM signal goes on CAT6 through the second EtherCON connector to the LED
daughter board. An IRL520 MOSFET on that board controls the current in the 12V
LED strip. The result is the same as the analog PCB of the first system. But
the firmware calculates it.

## Firmware

### Files

The firmware in use is in `src/teensy_codebase/teensy_code_v3`:

```
config.h            pins, times, status codes and protocol constants
StatusLeds.h/.cpp   4-LED binary status display
Relays.h/.cpp       amplifier and speaker relay sequence
AudioEngine.h/.cpp  SGTL5000 codec, SD card playback, peak and RMS analyzers
RtcClock.h/.cpp     DS3231 clock (leader only)
SerialLink.h/.cpp   addressed frames with a checksum, on Serial3
Controller.h/.cpp   role detection, leader and follower state machines
teensy_code_v3.ino  entry point, makes one Controller
```

The `Controller` object holds one instance of each subsystem. It is the only
global object.

`ledzCtrl.h` and `sysCtrl.h` are also in this folder. They are old files from
V2. No file includes them. They are not part of the firmware in use.

`RTC_setup_code/RTC_setup_code.ino` is a separate sketch. Use it to test or to
program a DS3231 module away from the installation.

### Build

1. Start the Arduino IDE with Teensyduino.
2. Select Tools > Board > Teensy 4.0.
3. Select the port of the unit.
4. Click Upload.

The board selection is important. `Watchdog_t4.h` declares `WDT_T4` and `WDOG1`
for the i.MX RT chip only. Teensyduino keeps the 3.x boards and the 4.x boards
in the same `avr` platform folder. Therefore a build with a Teensy 3.x board
gives this error: `'WDOG1' was not declared in this scope`. The library is
correctly installed, but the header is empty for that board.

All three units use the same compiled sketch.

### Compile-time flags

The flags are at the top of `config.h`.

```
USE_WATCHDOG   1  the hardware watchdog resets the unit after 15 seconds
                  of a hang. Set to 0 only if the WDT_T4 library is absent.
                  It must be 1 on site.
AUDIO_TRACE    0  the leader sends an audio-health line each second while
                  audio plays. Set to 1 for diagnostics only. It must be 0
                  on site.
```

`cfg::AUDIO_MEMORY_BLOCKS` (120) sets the size of the audio buffer pool. One
block is 128 samples, or 2.9 milliseconds of audio. The value is larger than
necessary. This gives protection against a slow read from the SD card.

### Role detection

`Controller::detectRole()` reads the three strap pins one time at start-up:

```
pin 32 HIGH  ->  LEADER (LONG)
pin 30 HIGH  ->  SMALL
pin 28 HIGH  ->  SEASHELL
```

The pins use the internal pulldown resistors. Therefore a disconnected strap
reads LOW. If no pin reads HIGH, the unit becomes the leader and sends a
warning on the USB console.

## Teensy pinout

| Pin | Function |
|---|---|
| 0, 1 | Reserved for USB Serial. Do not connect. |
| 2-5 | Status LEDs 1-4 (binary array, LED 1 = MSB) |
| 6 | PWM output to the LED strip driver |
| 7, 8 | Not in use (Serial2 is possible) |
| 9, 12 | Not in use |
| 10 | SD card CS |
| 11 | SD card MOSI |
| 13 | SD card SCK |
| 14 | Serial3 RX (SMALL and SEASHELL only) |
| 15 | Serial3 TX (LONG only) |
| 16 | Relay 1, speaker |
| 17 | Relay 2, 36V PSU and amplifier |
| 18 | RTC SDA (I2C, LONG only) |
| 19 | RTC SCL (I2C, LONG only) |
| 20 | Audio Shield LRCLK |
| 21 | Audio Shield BCLK |
| 22 / A8 | Volume potentiometer analog input |
| 23 | Audio Shield MCLK |
| 28 | Hardware ID strap, SEASHELL |
| 30 | Hardware ID strap, SMALL |
| 32 | Hardware ID strap, LONG (leader) |

Serial3 and the PWM line go through EtherCON chassis sockets (NE8FDPU-TOP) on
CAT6.

The RJ45 pinout of the serial link is opposite at the two ends of the run.
Therefore the Tx of the leader goes to the Rx of the follower. One end is 1 Rx,
2 GND, 3 Tx. The other end is 1 Tx, 2 GND, 3 Rx. Pins 4 to 8 are not in use.

Refer to page 8 of `documentation/manual/manual v1.0.pdf` before you make a
connector again.

PWM and volume RJ45 on LONG: 1 NC, 2 GND, 3 NC, 4 +3.3V, 5 A8 potentiometer
signal, 6 NC, 7 PWM+, 8 PWM-.

PWM RJ45 on SMALL and SEASHELL: 1 to 6 NC, 7 PWM+, 8 PWM-.

## Serial3 link protocol

The three units share one Serial3 link at 9600 baud. The forward line goes from
the Tx of the leader to the Rx of the two followers. The return line goes from
the Tx of the two followers to the Rx of the leader. Therefore only the return
line is common to more than one transmitter.

Each frame has an address and a checksum:

```
#<addr><cmd><arg>*<XX>\n
```

`addr` is `0` for the two followers, `1` for SMALL, `2` for SEASHELL, or `L`
for the leader. Only status replies use `L`.

`cmd` is one command character. `arg` is an optional short ASCII payload.

`XX` is the XOR of all the characters between `#` and `*`, in two hexadecimal
digits.

`SerialLink::poll()` makes one frame at a time and does not block. It ignores a
frame with an overflow, with no `*`, or with an incorrect checksum. A `#`
character always starts a new frame.

### Command characters

| Char | Command | Notes |
|---|---|---|
| `P` | Play | The leader sends this one time for each round, to keep the units synchronized |
| `X` | Stop | Stops the audio and sets the PWM output to zero |
| `W` | Wake | Does the relay wake sequence: 36V PSU on, then speaker on |
| `S` | Sleep | Does the relay sleep sequence: speaker off, then 36V PSU off |
| `H` | Heartbeat | Leader to followers each 5 seconds. `arg` is `1` or `0` for the awake state |
| `R` | Report request | The leader sends this to one follower. That follower answers one time |
| `s` | Status reply | Follower to leader only. `arg` is `role,awake,playing,cpu_temp` |
| `B` | Reboot | The leader sends this, then each unit resets after 3 seconds |
| `V` | Set volume | `arg` is a value between 0.0 and 1.0 |
| `+` / `-` | Volume up or down | Steps of 0.1 |
| `>` / `<` | PWM range up or down | Steps of 25, between 0 and 255 |
| `K` | Volume knob on or off | Lets the potentiometer control the volume |
| `?` | Help | Prints the command list on the USB console only |

### How the two followers share one line

The two followers transmit on the same return line. Three rules prevent two
transmissions at the same time.

- Only `R` causes a follower to transmit. The follower does all other commands
  and stays quiet.
- The leader never sends `R` to address `0`. It always sends `R` to `1` or to
  `2`. The other follower reads the address, and stops.
- The automatic poll asks SMALL, waits 150 milliseconds
  (`cfg::REPORT_STAGGER_MS`), then asks SEASHELL.

A request and a reply together are approximately 25 milliseconds at 9600 baud.
Therefore the 150 millisecond interval is sufficient.

If two replies do come at the same time, the checksum fails. The leader ignores
the frame. The next automatic poll is 20 seconds later.

## USB serial console

Connect the USB cable to a unit. Then open a serial terminal at 9600 baud. The
Arduino Serial Monitor, PuTTY or other serial tools are all satisfactory.
The enclosure can be open or closed.

This is the primary method to communicate with one unit. It does not use the
Serial3 link.

### Single-key commands

```
P  play          W  wake      R  report    +  volume up     >  PWM range up
X  stop          S  sleep     B  reboot    -  volume down   <  PWM range down
K  volume knob on/off         ?  help
```

On the leader, the keys `P X W S + - > < B` go to the followers first. Then the
leader does the command. Therefore the volume keys and the PWM keys change all
three units together.

The keys `R`, `K` and `?` operate on the local unit only. On a follower, all
keys operate on that follower only.

`R` prints the role, the WAV file name, the RTC time on the leader, the awake
and playing states, the volume, the PWM range, the number of rounds, the codec
and SD card status, and the CPU temperature.

### Line commands

A line command starts with `:`. Enter or a pause of 200 milliseconds completes
it.

Clock commands:

```
:settime YYYY MM DD HH MM SS   set the RTC directly
:synctime                      set the RTC to the compile time of the sketch
:time                          print the RTC time
```

The clock commands are for the leader. On a follower the RTC is not
initialized, and the command does nothing.

`:synctime` is a last option only. The compile time is old by the interval
between the build and the upload.

Follower status commands. The leader accepts these commands only:

```
:small      request a status report from SMALL
:seashell   request a status report from SEASHELL
:status     request a report from the two followers, one after the other
```

The follower answers a moment later. The reply prints on the console of the
leader in this form:

```
Status from SMALL: awake YES, playing NO, CPU 41.7 C
```

The leader also requests these reports automatically each 20 seconds.

### Volume

The volume control is not linear. `AudioControlSGTL5000::volume()` writes the
headphone register of the codec. That register is in steps of 0.5 dB, from
+12 dB to -51.5 dB.

```
1.0  ->  +12.0 dB      0.6  ->  -13.5 dB   (the value on site)
0.9  ->   +6.0 dB      0.5  ->  -20.0 dB
0.8  ->   -0.5 dB      0.3  ->  -32.5 dB
0.7  ->   -7.0 dB      0.1  ->  -45.5 dB
```

Each step of 0.1 is therefore approximately 6.5 dB. A value of 1.0 gives 12 dB
of gain, not the level of the audio file.

The unit does not keep the volume value. At each start-up the volume returns to
`cfg::DEFAULT_VOLUME`.

## Status LED codes

The 4-LED array on each motherboard shows a binary code. LED 1 is the most
significant bit. LED 4 is the least significant bit. In the LEDs column, `1` is
an LED that is on.

| Code | LEDs | Name | Meaning |
|---|---|---|---|
| 1 | 0001 | SLEEP | asleep, normal |
| 2 | 0010 | AWAKE | awake, no audio |
| 3 | 0011 | SD_ERR | no SD card |
| 4 | 0100 | CODEC_ERR | no SGTL5000 codec |
| 5 | 0101 | RTC_ERR | no clock (leader only) |
| 6 | 0110 | LINK_ERR | no heartbeat for 90 seconds (followers only) |
| 7 | 0111 | REBOOT | the unit starts again |
| 8 | 1000 | PLAYING | audio plays |

Code 1 during the sleep hours, and sound and light during the awake hours, are
the normal condition.

### Fault codes at start-up

The unit shows the fault codes SD_ERR, CODEC_ERR and RTC_ERR at start-up only.
It does not keep them on the display.

`setup()` finds these faults. At the end of `setup()`, the unit flashes each
fault code for 4 seconds. Then the normal state display starts.

The flash is necessary. A continuous code looks the same as a normal state
code. Also, the first cycle of `loop()` writes to the LED array immediately.

A unit that operates for some hours therefore shows a normal code. This happens
even if the SD card is defective. To read the fault codes again, start the unit
again. Refer to "Read the fault codes at start-up".

## Daily schedule

The leader reads the RTC each 30 seconds. If the awake state must change, the
leader sends `W` or `S` to the two followers. The heartbeat also carries the
awake state.

`config.h` has `cfg::START_HOUR = 8` and `cfg::END_HOUR = 20`. The units are
awake from 08:00 to 20:00. The printed manual gives the first schedule of 07:00
to 22:00. Correct one of the two documents.

To change the schedule, set `START_HOUR` and `END_HOUR` in `config.h`. Then
upload the firmware to the leader. The followers do not read an RTC. They take
the awake state from the leader.

The units stay energized during sleep. Only the 36V amplifier PSU and the
speaker relay go off. The MCU, the watchdog and the Serial3 link continue for
24 hours each day.

## Diagnostics

Set `AUDIO_TRACE` to `1` in `config.h`. The leader then sends one line each
second while audio plays. The default value is `0`.

```
t=<position ms> mem=<peak audio blocks> cpu=<peak audio ISR load %>
```

Read the output as follows:

- If `mem` increases to the limit of 120, the SD card is too slow.
- If `t` does not increase by about 1000 between two lines, the SD card is too
  slow.
- If `mem` and `t` stay correct during an audio fault, the digital circuit is
  correct. The cause is then in the analog circuit after the codec.

Record the output to a file. Use time stamps. Do not use the monitor window
only. An intermittent fault needs the lines before the event and after the
event. It also needs a count of the events before you make a change.

Set `AUDIO_TRACE` to `0` again after the test.

## Troubleshooting

### Read the fault codes at start-up

1. Connect the USB cable of the unit to a computer.
2. Open the enclosure.
3. Press `B` on the USB console. As an alternative, remove the power. Then
   apply the power again.
4. Look at the LED array during the start-up.
5. The unit flashes each fault code for 4 seconds.
6. If the unit flashes no code, the codec, the SD card and the RTC are
   serviceable.

### Code 3 (SD_ERR): no SD card

1. Make sure that the card is fully in the slot. The slot is below the Teensy
   board.
2. Make sure that the card has a FAT32 file system.
3. Make sure that the card has the correct file name for that unit: `LONG.WAV`,
   `SMALL.WAV` or `SEASHELL.WAV`.
4. Use the files from `src/audio/mono/`. Do not use the files from
   `src/audio/stereo/`.
5. The unit tries to find the card again each 3 seconds. It stops after 20
   tries, or approximately 60 seconds after the start-up. Therefore install the
   card again, then start the unit again.

### Code 4 (CODEC_ERR): no codec

1. Remove the Teensy Audio Shield. Then install it again on the header.
2. Examine the solder joints on the shield and on the header pins.
3. Examine the 3.3V supply to the shield.

### Code 5 (RTC_ERR): no clock

1. Examine the I2C wiring on pins 18 and 19. The leader only shows this code.
2. Examine the DS3231 module.
3. Connect the USB cable. If the leader reports lost power at each start-up,
   replace the coin cell.

### Code 6 (LINK_ERR): no heartbeat

1. Examine the CAT6 cable between the leader and the follower.
2. Examine the Serial3 connections at the two ends. The Tx and Rx pins are
   opposite at the two ends. Pin 2 is GND.
3. Make sure that the leader has power and operates. A follower shows code 6
   if the leader stops.
4. Send `:small` or `:seashell` on the USB console of the leader. If the
   follower answers, the cable is serviceable in the two directions.
5. If the follower does not answer, examine the power and the LED array of that
   follower.

### One unit makes no sound

1. Do the procedure "Read the fault codes at start-up".
2. Make sure that the two PSU indicators are on during the awake hours.
3. Examine the minijack cable from the Audio Shield to the RCA input of the
   amplifier.
4. Press `P` on the USB console. If the unit then plays, the schedule is the
   cause. Refer to "The unit sleeps at the incorrect time".
5. If the amplifier channel is defective, connect the second channel of the
   same board.

### The audio has noise or interruptions

1. Make sure that the SD card has the files from `src/audio/mono/`. A file with
   two channels needs two times more bandwidth.
2. Set `AUDIO_TRACE` to `1` in `config.h`. Then build the firmware and upload
   it.
3. Record the console output to a file for one hour. Use time stamps.
4. Write down the time of each audio fault.
5. Read the trace. Refer to "Diagnostics".
6. If the trace shows that the SD card is too slow, replace the SD card.
7. If the trace stays correct, connect headphones to the minijack output. Then
   listen again.
8. If the noise continues in the headphones, the cause is in the codec or its
   connections. Examine the Audio Shield.
9. If the headphones are correct, the cause is in the amplifier or the speaker.
10. Set `AUDIO_TRACE` to `0` again after the test.

### The unit sleeps at the incorrect time

1. Connect the USB cable to the leader.
2. Send `:time` on the USB console.
3. If the time is incorrect, send `:settime YYYY MM DD HH MM SS` with the
   correct values.
4. If the leader reports lost power at each start-up, replace the DS3231 coin
   cell.
5. Make sure that `START_HOUR` and `END_HOUR` in `config.h` are correct.

### Relay problems

1. Two of the four relays are in use. Relay 1 is the speaker. Relay 2 is the
   36V PSU.
2. If one relay becomes defective, connect one of the two spare relays on the
   same board.
3. If all four relays become defective, replace the board.

### The unit does not answer on USB, but the LEDs operate

1. The watchdog resets the unit after 15 seconds of a hang. Wait 20 seconds.
2. If the unit stays locked, the cause is probably the hardware. The main loop
   does not block.
3. Examine the USB cable and the USB connector.

### The build stops with an error

1. If the error is `'WDOG1' was not declared in this scope`, look at the board
   selection. It must be Teensy 4.0.
2. If the board selection is correct, install the WDT_T4 library. As an
   alternative, set `USE_WATCHDOG` to `0` for a test build.
3. Look at the full path in the error message. Make sure that it is the path of
   your local repository. A different copy of the sketch can have different
   values in `config.h`.
4. Close the Arduino IDE and open it again. The IDE can keep an old object file
   in its cache.

## Required toolkit

- Flat screwdriver 3 mm for the terminal blocks.
- Flat screwdriver 6 mm and Phillips 6 mm for the screws of the enclosures and general use.
- Multimeter for voltage and continuity tests: 230V AC, 12V DC, 36V DC.
- Phillips 000 and 00 screwdrivers, and 4 mm and 4.5 mm hex keys.
- Anti-static wrist strap.
- Wago connectors, wire and crocodile clips for temporary connections.
- 3.5 mm stereo headphones to listen to the Audio Shield output directly.
- A computer with a USB-A port, the Arduino IDE with Teensyduino, and a serial
  monitor.
- A long CAT6 test cable, or a cable tester, for the underground runs.

For workshop tests:

- An Arduino Uno R3 to program or test a DS3231 module. Refer to
  `RTC_setup_code`.
- An oscilloscope for signal analysis.
- A spare Teensy 4.0 to exchange components.
- A bench power supply for isolated tests.

## Warranty and support

Warranty period: 04.06.2025 to 04.06.2026.

Support during the warranty period: ahureau@pm.me, or the support technician on
site in Turku. The usual answer time is 24 to 48 hours.

## References

- `documentation/manual/manual v1.0.pdf` is the full technical manual. It has
  the block diagrams and the connector pinout charts. This README comes from
  that manual.
- `documentation/editable doc.drawio` and `editable doc v2.drawio` are the
  editable source diagrams.
- `documentation/3D prints stl files/` has the enclosure gaskets and the
  mounting parts.
- (UNUSED)`documentation/schematics v2.pdf` is for the analog envelope-follower PCB of
  the first Raspberry Pi system. It does not apply to this Teensy system. The
  firmware now calculates the light envelope.
