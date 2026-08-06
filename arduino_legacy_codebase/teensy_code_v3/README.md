# Suuret Muinaiset - V3 firmware

Teensy 4.0 firmware for the 3-unit sound-and-light installation. All three
units run identical firmware; the role (leader / follower) is selected by
the hardware ID strap pins.

## File layout

```
config.h            compile-time pins, timings, status codes, protocol
StatusLeds.h/.cpp   4-LED binary status display
Relays.h/.cpp       amp + speaker power sequencing
AudioEngine.h/.cpp  SGTL5000 codec, SD WAV playback, peak/RMS analyzers
RtcClock.h/.cpp     DS3231 wrapper (leader only)
SerialLink.h/.cpp   addressed, checksummed Serial3 framing
Controller.h/.cpp   role detection + leader/follower state machine
main.cpp            entry point (setup / loop)
platformio.ini      PlatformIO build
```

`Controller` owns one instance of every subsystem. `main.cpp` creates one
`Controller` and forwards `setup()` / `loop()` to it. Nothing else is global.

## Serial protocol (Serial3)

```
#<addr><cmd><arg>*<XX>\n
```

`addr` is `0` (all followers), `1` (small), `2` (seashell) or `L` (leader,
for replies). `XX` is the 2-hex XOR of `addr+cmd+arg`. Only the addressed
unit transmits, so the shared back-channel never collides. Malformed frames
are dropped without blocking.

## USB console

Single keys: `P` play, `X` stop, `W` wake, `S` sleep, `R` report, `B`
reboot, `+`/`-` volume, `>`/`<` PWM range, `K` knob toggle, `?` help.
On the leader these also relay to the followers.

Line commands: `:settime YYYY MM DD HH MM SS`, `:synctime`, `:time`.

## First-run checklist

1. `RELAY_ACTIVE_LOW` in `config.h` - flip to `true` if the relays energize
   at boot or behave inverted (opto-isolated boards).
2. RTC coin cell - if the leader keeps reporting lost power, replace it.
3. SD card holds `LONG.WAV`, `SMALL.WAV`, `SEASHELL.WAV` (8.3 names).
