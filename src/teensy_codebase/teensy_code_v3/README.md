# Suuret Muinaiset — V3 firmware

Teensy 4.0 firmware for the three-unit sound-and-light installation. All three
units run identical firmware; the role is selected at boot by the hardware ID
strap pins, so a unit can be swapped in the field without a re-upload.

## Architecture

One `Controller` owns one instance of every subsystem. The sketch creates a
single `Controller` and forwards `setup()` / `loop()` to it. Nothing else is
global.

```
config.h            compile-time pins, timings, status codes, protocol
StatusLeds.h/.cpp   4-LED binary status display
Relays.h/.cpp       amp + speaker power sequencing
AudioEngine.h/.cpp  SGTL5000 codec, SD WAV playback, peak/RMS analyzers
RtcClock.h/.cpp     DS3231 wrapper (leader only)
SerialLink.h/.cpp   addressed, checksummed Serial3 framing
Controller.h/.cpp   role detection + leader/follower state machines
teensy_code_v3.ino  entry point
```

### Leader and followers

**LONG is the leader.** It holds the only clock and the only schedule. Every
30 s it asks the RTC whether the installation should be awake and, on a change,
broadcasts wake or sleep before applying it locally. Every 5 s it repeats the
current awake state as a heartbeat, so a follower that rebooted mid-cycle
resynchronizes on its own. While awake it drives rounds: when nothing is
playing and the inter-round gap has elapsed, it broadcasts `PLAY` and starts
its own file. Rounds are re-broadcast every cycle rather than started once, so
the three units resynchronize continuously instead of drifting over a day.

**SMALL and SEASHELL are followers.** They hold no schedule and no timers of
their own — they act on what arrives and report when addressed. Past 90 s with
no heartbeat they keep doing whatever they were doing but show `LINK_ERR`, so a
broken cable is visible on the LED array without changing the piece.

### Audio path

The WAV files are 16-bit stereo PCM at 44.1 kHz, but **only the left channel is
played**. It is fanned out to both I2S channels and to both analyzers, so the
light follows exactly what is heard. This is deliberate, and it means the files
are expected to be dual mono — worth re-checking after any re-render.

`AudioPlaySdWav` refills its buffer from inside the audio interrupt, not from
`loop()`. So a slow main loop stutters the light, not the sound; but anything in
the main context that touches the card competes with the interrupt for it. That
is why `retryInit()` refuses to run while audio is playing. The Teensy 4.0 has
no built-in SDIO slot, so the card is necessarily on the Audio Shield's SPI
socket and sustained read latency is the realistic failure mode.

## Serial protocol (Serial3)

```
#<addr><cmd><arg>*<XX>\n
```

`addr` is `0` (all followers), `1` (small), `2` (seashell) or `L` (leader, for
replies). `XX` is the 2-hex XOR of `addr+cmd+arg`. All three units share one
half-duplex pair over buried CAT6; only the addressed unit transmits, so the
back-channel never collides. Malformed frames are dropped without blocking, and
a stray `#` restarts the frame rather than wedging the parser.

## USB console

Single keys: `P` play, `X` stop, `W` wake, `S` sleep, `R` report, `B` reboot,
`+`/`-` volume, `>`/`<` PWM range, `K` knob toggle, `?` help.

On the leader, `P X W S + - > < B` are relayed to the followers before being
applied locally — so volume and PWM keys typed on the leader move all three
units together. `R` reports locally only.

Line commands: `:settime YYYY MM DD HH MM SS`, `:synctime`, `:time`.

## Diagnostics

While the leader is playing it emits one line per second:

```
t=<position ms> mem=<peak audio blocks> cpu=<peak audio ISR load %>
```

`mem` climbing toward the `AudioMemory()` ceiling (120), or `t` failing to
advance by roughly 1000 between lines, means the SD path is starving. Both
staying healthy through an audible fault means the digital side is delivering
correctly and the problem is downstream in the analog chain.

Log with timestamps enabled and to a file, not to the scrolling monitor — for
an intermittent fault you need the lines on both sides of an episode, and a
baseline episode rate before you change anything.

## Build

Arduino IDE with Teensyduino. **Tools → Board → Teensy 4.0**, select the port,
upload.

The board target matters. `Watchdog_t4.h` only declares `WDT_T4` and `WDOG1`
for the i.MX RT chip, and Teensyduino files 3.x and 4.x under the same `avr`
platform folder — so building with a Teensy 3.x selected strips the header to
nothing and fails with `'WDOG1' was not declared in this scope`, even though
the library is installed correctly.

## First-run checklist

1. `RELAY_ACTIVE_LOW` in `config.h` — flip to `true` if the relays energize at
   boot or behave inverted (opto-isolated boards).
2. RTC coin cell — if the leader keeps reporting lost power, replace it.
   Without a backup cell the schedule resets at every power cycle.
3. SD card holds `LONG.WAV`, `SMALL.WAV`, `SEASHELL.WAV` (8.3 names).
4. `USE_WATCHDOG` in `config.h` — must be `1` before the units are left
   running unattended on site.

## Known discrepancies

- Technical manual v1.0 documents a 07:00–22:00 schedule; `config.h` compiles
  `START_HOUR 8` / `END_HOUR 20`. Reconcile before the next site visit.
- `sysCtrl.h` and `ledzCtrl.h` are V2 leftovers still sitting in this folder.
  Nothing includes them, but they should be deleted.
