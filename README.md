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

Each creature also has its own analog circuit converting its audio channel's amplitude into LED brightness, independent of the main speaker signal path. In progress.

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

The script is triggered daily by root's crontab (sudo crontab -e). Current entry: 00 22 * * * /home/mike/sleep_pi.sh, meaning cron fires the sleep script at 22:00, and the script sets the wake alarm for 08:00 the next day. When changing sleep or wake times, update both the cron line and the date string in the script together, leaving a few minutes of margin between the cron trigger and the wake target.

### Autostart

The Pi boots into the desktop automatically (autologin enabled via raspi-config, System Options, Boot/Auto Login, Desktop Autologin). On desktop load, labwc reads ~/.config/labwc/autostart, which contains:

lxterminal -e "/usr/bin/pd -nogui /home/mike/Documents/suuret_muinaiset/pd_codebase/main.pd" &

The trailing & is required so labwc doesn't wait for PD to exit before finishing desktop startup. This opens a visible terminal running PD, so a technician with a screen and keyboard can see live output directly, while remote access via Pi Connect or SSH works the same regardless.

### Daily cycle

08:00, Pi wakes, boots to desktop, PD autostarts via labwc.

08:00 to 22:00, PD runs continuously, looping playback through all three channels.

22:00, cron fires sleep_pi.sh, which sets the next wake alarm and shuts the Pi down fully.

22:00 to 08:00, Pi is fully powered off.

## Network Access

The Pi 5 runs a permanent local WiFi access point via its onboard chip (wlan0), allowing on-site technicians to connect and SSH into the system without opening the enclosure or requiring venue network access.

A USB WiFi dongle (wlan1) is available as a client interface. When remote support is needed, a technician can connect wlan1 to a phone hotspot via SSH, bringing the Pi online so that Pi Connect becomes available for remote access.

### Access point details

SSID: HUOLTO-01 (hidden, not broadcast)
Security: WPA2
Credentials: see project password manager

### Connecting

Connect to HUOLTO-01 from a device with an SSH client. The Pi is reachable at 10.42.0.1 (NetworkManager hotspot default).

```
ssh ant1@10.42.0.1
```

### Enabling remote access on-site

Once connected via SSH over the AP, connect wlan1 to a hotspot:

```
sudo nmcli dev wifi connect "HotspotName" password "hotspotpassword" ifname wlan1
```

Pi Connect will become available automatically once wlan1 has internet.

---

## Pure Data — Command Line Options

The installation launches Pd headless with explicit audio configuration flags set in the autostart command, rather than relying on Pd's saved preferences. This ensures consistent behaviour across reboots and system changes.

Current launch flags:

```
pd -nogui -alsa -alsaadd snd_rpi_hifiberry_dac8x -outchannels 8 -noadc -nomidi -r 44100 -blocksize 512 -audiobuf 25 -noprefs -open /path/to/patch.pd
```

Flag summary:

| Flag | Purpose |
|---|---|
| `-nogui` | Headless operation |
| `-alsa` | Force ALSA audio API |
| `-alsaadd snd_rpi_hifiberry_dac8x` | Pin HiFiBerry DAC8x by name (stable across reboots) |
| `-outchannels 8` | Open all 8 DAC channels |
| `-noadc` | Disable audio input |
| `-nomidi` | Disable MIDI |
| `-r 44100` | Sample rate |
| `-blocksize 512` | DSP block size in samples |
| `-audiobuf 25` | Audio I/O buffer in milliseconds |
| `-noprefs` | Ignore saved Pd preferences; command line is authoritative |

Full reference for Pd command line options: https://puredata.info/docs/faq/commandline

## Toolkit required for maintenance

Screwdrivers to open/close boxes and fasten terminal blocks: flat 3mm, flat 6mm, Philips 6mm.

Multimeter to test voltage, current, and continuity.

Wago connectors and crocodile cables for temporary test connections.

A pair of headphones with minijack, to test audio output directly from the Pi (DAC 5/6)

HDMI cable, screen, keyboard and mouse, for direct access to the Pi's desktop on site.

Computer or laptop for SSH/Pi Connect access when remote troubleshooting is preferred over an on-site screen.

A smartphone with hotspot to connect the Pi to the wifi, allowing remote control via Pi-Connect

Long Cat6 testing cable or cable tester, for diagnosing the underground transformer runs.

Gaffer tape, zip ties, and miscellaneous crimping tools can be helpful.
