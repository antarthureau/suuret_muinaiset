# suuret_muinaiset system (V2)
## Overview
The installation runs on a single Raspberry Pi 5, replacing the previous three-unit Teensy-based system (LONG, SMALL, SEASHELL).
One Raspberry Pi 5 running Pure Data plays three audio files simultaneously (main.pd in pd_codebase), one file per creature, and drives both the speakers and the LED lighting (sound-to-light analog circuit) for all three units.

## Hardware/Software Requirements
Raspberry Pi 5 with onboard RTC and battery, used for the daily sleep/wake cycle.

Pure Data (Pd), run headless via pd -nogui. Login at desktop activated, so troubleshooting with mouse, keyboard and HDMI screen is possible.

zexy external library (installed via Deken), loaded in the patch with [declare -lib zexy].

HiFiBerry 8-channel DAC card, three of its outputs used for the three audio channels. LONG on DAC3, SMALL on DAC1, SEASHELL on DAC2, and DAC5/6 is a headphone out with a sum of all creatures.

Neutrik transformers, used twice per channel: once locally to balance the signal for transmission, and once at each remote creature to convert back to unbalanced before the amp.

Cat6 cable, run underground, carrying the balanced signal to each creature's site.

Amplifier and speaker at each creature site.

Discrete analog envelope-follower/MOSFET circuit per creature (two BC547 transistors plus an IRL540 MOSFET) converting that creature's audio amplitude into LED strip brightness.

12V LED strip per creature (final installation voltage)

2* SMPS per unit: 36VDC for the amplifier and 12VDC for the LED strips.

## Signal path

Pd plays three looping audio files, one per creature, out through three channels of the HiFiBerry DAC. Each unbalanced line passes through a Neutrik transformer to balance it for the long underground Cat6 run to that creature's location. At the far end, a second Neutrik transformer converts the signal back to unbalanced before it reaches that creature's amplifier and speaker.

## Audio-to-light circuit

Each creature also has its own analog circuit converting its audio channel's amplitude into LED brightness, independent of the main speaker signal path. Two BC547 stages provide double inversion to restore correct polarity, feeding a half-wave rectifier and then the IRL540 MOSFET gate, which switches the 12V LED strip. Wiring convention: MOSFET drain to LED strip negative, source to GND. Target envelope decay is 100 to 200ms.

Status: in progress. Pending items include resolving gate voltage headroom on the envelope-follower output, confirming correct MOSFET orientation in the final build, and moving from the 24V breadboard test supply to the 12V installation supply. A professional circuit design was also commissioned in parallel and may be evaluated against this build.

## Pure Data patch

Main patch: pd_codebase/main.pd, run in nogui mode.

Loops playback continuously; prints a status line at the start of each loop, captured by the terminal/log rather than the patch itself doing timestamping.

Elapsed time within each loop is tracked via zexy's realtime object, converted to mn:ss:ms via expr and sprintf, and reset at the start of each playback cycle (roughly every 6 minutes).

Remote control of the running patch (e.g. via Pd's netreceive) is possible but not yet implemented.

## Sleep/Wake schedule

The Pi sleeps and wakes itself daily using its onboard RTC and battery, fully powering off rather than suspending. This requires POWER_OFF_ON_HALT=1 in the bootloader config, checked with sudo rpi-eeprom-config.

The script /home/mike/sleep_pi.sh sets the next wake alarm and shuts the system down:

#!/bin/bash
echo 0 > /sys/class/rtc/rtc0/wakealarm
echo $(date -d "tomorrow 08:00" +%s) > /sys/class/rtc/rtc0/wakealarm
/usr/sbin/shutdown -h now

It must use the full path /usr/sbin/shutdown, since cron runs with a minimal PATH that doesn't include /usr/sbin.

The script is triggered daily by root's crontab (sudo crontab -e). Current entry: 15 15 * * * /home/mike/sleep_pi.sh, meaning cron fires the sleep script at 15:15, and the script sets the wake alarm for 08:00 the next day. When changing sleep or wake times, update both the cron line and the date string in the script together, leaving a few minutes of margin between the cron trigger and the wake target.

### Autostart

The Pi boots into the desktop automatically (autologin enabled via raspi-config, System Options, Boot/Auto Login, Desktop Autologin). On desktop load, labwc reads ~/.config/labwc/autostart, which contains:

lxterminal -e "/usr/bin/pd -nogui /home/mike/Documents/suuret_muinaiset/pd_codebase/main.pd" &

The trailing & is required so labwc doesn't wait for PD to exit before finishing desktop startup. This opens a visible terminal running PD, so a technician with a screen and keyboard can see live output directly, while remote access via Pi Connect or SSH works the same regardless.

### Daily cycle

08:00, Pi wakes, boots to desktop, PD autostarts via labwc.

08:00 to 15:15, PD runs continuously, looping playback through all three channels.

15:15, cron fires sleep_pi.sh, which sets the next wake alarm and shuts the Pi down fully.

15:15 to 08:00, Pi is fully powered off.

## Toolkit required for maintenance

Screwdrivers to open/close boxes and fasten terminal blocks: flat 3mm, flat 6mm, Philips 6mm.

Multimeter to test voltage, current, and continuity.

Oscilloscope, for checking envelope-follower and gate voltages on the analog LED circuits.

Breadboard and Falstad simulator access, for testing/iterating on the analog circuit before field changes.

Wago connectors and crocodile cables for temporary test connections.

A pair of headphones with minijack, to test audio output directly from the Pi.

HDMI cable, keyboard and mouse, for direct access to the Pi's desktop on site.

Computer or laptop for SSH/Pi Connect access when remote troubleshooting is preferred over an on-site screen.

Long Cat6 testing cable or cable tester, for diagnosing the underground transformer runs.

Gaffer tape, zip ties, and miscellaneous crimping tools can be helpful.
