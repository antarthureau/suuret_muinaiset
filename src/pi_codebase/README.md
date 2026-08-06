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

Custom analog PCB per creature: envelope-follower circuit based on TL072 and TL071 op-amps with an IRL540 MOSFET, converting that creature's audio amplitude into LED strip brightness.

12V LED strip per creature (final installation voltage).

2× SMPS per unit: 36VDC for the amplifier and 12VDC for the LED circuit PCB and LED strips. On the LONG unit, the PCB also carries an on-board Traco DC-DC converter providing 5V to power the Raspberry Pi from the 12V supply rail; this component is not populated on the SMALL and SEASHELL boards.

## Signal path

Pd plays three looping audio files, one per creature, out through three channels of the HiFiBerry DAC. Each unbalanced line passes through a Neutrik transformer to balance it for the long underground Cat6 run to that creature's location. At the far end, a second Neutrik transformer converts the signal back to unbalanced before it reaches that creature's amplifier and speaker.

## Audio-to-light circuit

Each creature has a custom PCB (ordered from JLCPCB) that converts its audio channel's amplitude into a brightness level for its 12V LED strip, independently of the speaker signal path.

Signal path through the circuit:

Audio arrives at an RCA connector and passes through a 1µF capacitor (AC coupling, blocking any DC offset) into a bias network — a 100kΩ/100kΩ resistor divider sets a +6V virtual midpoint from the 12V supply, centering the signal for single-supply op-amp operation.

The first TL072 half (U1A) buffers the input. The second TL072 half (U1B) amplifies the signal; gain is set by a 1MΩ feedback resistor and a 100kΩ trimmer potentiometer.

A 1N4148 diode half-wave rectifies the amplified AC signal into a positive envelope voltage. Two 100kΩ trimmer potentiometers control attack time (how fast the envelope rises with loud transients) and decay time (how slowly it falls when sound stops). A 4.7µF capacitor holds the peak level between transients.

A TL071 (U2) buffers the envelope voltage, isolating the capacitor from the output stage.

An IRL540 power MOSFET (Q1) is driven through a 100Ω gate resistor. A 10kΩ pull-down resistor on the gate ensures the MOSFET turns fully off when the signal drops to zero. The MOSFET switches the 12V LED strip current in proportion to the envelope voltage.

Power input is protected against reverse polarity by a 1N4007 diode. On the LONG unit PCB only, a Traco DC-DC converter provides a regulated 5V output used to power the Raspberry Pi from the 12V supply rail. This component is not populated on the SMALL and SEASHELL boards.

## Pure Data patch

Main patch: pd_codebase/main.pd, run in nogui mode.

Loops playback continuously; prints a status line at the start of each loop, captured by the terminal/log rather than the patch itself doing timestamping.

Elapsed time within each loop is tracked via zexy's realtime object, converted to mn:ss:ms via expr and sprintf, and reset at the start of each playback cycle (roughly every 6 minutes).

Remote control of the running patch (e.g. via Pd's netreceive) is possible but not yet implemented.

## Sleep/Wake schedule

The Pi sleeps and wakes itself daily using its onboard RTC and battery, fully powering off rather than suspending. This requires POWER_OFF_ON_HALT=1 in the bootloader config, checked with sudo rpi-eeprom-config.

The script /home/mike/sleep_pi.sh sets the next wake alarm and shuts the system down:

```
#!/bin/bash
echo 0 > /sys/class/rtc/rtc0/wakealarm
echo $(date -d "tomorrow 08:00" +%s) > /sys/class/rtc/rtc0/wakealarm
/usr/sbin/shutdown -h now
```

It must use the full path /usr/sbin/shutdown, since cron runs with a minimal PATH that doesn't include /usr/sbin.

The script is triggered daily by root's crontab (sudo crontab -e). Current entry: 00 22 * * * /home/mike/sleep_pi.sh, meaning cron fires the sleep script at 22:00, and the script sets the wake alarm for 08:00 the next day. When changing sleep or wake times, update both the cron line and the date string in the script together, leaving a few minutes of margin between the cron trigger and the wake target.

### Autostart

The Pi boots into the desktop automatically (autologin enabled via raspi-config, System Options, Boot/Auto Login, Desktop Autologin). On desktop load, labwc reads ~/.config/labwc/autostart, which contains:

```
lxterminal -e "/usr/bin/pd -nogui -alsa -alsaadd snd_rpi_hifiberry_dac8x -outchannels 8 -noadc -nomidi -r 44100 -blocksize 512 -audiobuf 25 -noprefs -open /home/mike/Documents/suuret_muinaiset/pd_codebase/main.pd" &
```

The trailing & is required so labwc doesn't wait for Pd to exit before finishing desktop startup. This opens a visible terminal running Pd, so a technician with a screen and keyboard can see live output directly, while remote access via Pi Connect or SSH works the same regardless.

### Daily cycle

08:00, Pi wakes, boots to desktop, Pd autostarts via labwc.

08:00 to 22:00, Pd runs continuously, looping playback through all three channels.

22:00, cron fires sleep_pi.sh, which sets the next wake alarm and shuts the Pi down fully.

22:00 to 08:00, Pi is fully powered off.

## Network Access

The Pi has three network interfaces, each serving a different purpose:

wlan0 — onboard WiFi, configured as a permanent hidden-SSID access point. Used for on-site wireless maintenance without opening the enclosure. No internet access.

eth0 — wired ethernet, static IP. Used for a direct on-site cable connection when WiFi isn't practical or reliable enough.

wlan1 — USB WiFi dongle (D-Link AC13U). Connects outward to an external network (e.g. a phone hotspot) to bring the Pi online, which is required for Pi Connect remote support.

Pick the option below that matches your situation.

### Option 1: WiFi access point (on-site, no cable needed)

SSID: HUOLTO-NETT (hidden, not broadcast)
Security: WPA2
Password: see project credentials
Static IP: 10.42.0.1

On your laptop, open WiFi settings and manually join the hidden network HUOLTO-NETT (you'll need to enter the SSID by name since it won't appear in scan results). Set your laptop's WiFi interface to a static IP on the same subnet, e.g. 10.42.0.50 / 255.255.255.0.

Then:
```
ssh mike@10.42.0.1
```

### Option 2: Wired ethernet (on-site, direct cable)

Static IP: 10.42.1.2

Connect a Cat6 cable directly between your laptop's ethernet port (or USB/Thunderbolt-to-RJ45 adapter) and the Pi's ethernet port. Set your laptop's ethernet interface to a static IP on the same subnet, e.g. 10.42.1.50 / 255.255.255.0.

Then:
```
ssh mike@10.42.1.2
```

Useful when WiFi conditions on-site are poor, or as a fallback if the access point config is ever broken and needs fixing from scratch.

### Option 3: Internet access for remote support (wlan1)

If remote support from the development team is needed, wlan1 must be connected to an internet-providing WiFi network (e.g. a phone hotspot). Get in touch with the development team in advance so everyone involved can plan for a maintenance session.

Once you have SSH access via Option 1 or 2 above, connect wlan1:
```
sudo nmcli device wifi connect "hotspot_name" password "hotspot_password" ifname wlan1
```

If the hotspot password contains special characters, use single quotes instead of double quotes around it, otherwise the shell may misinterpret characters like $ or ( ) before nmcli ever sees them:
```
sudo nmcli device wifi connect "hotspot_name" password 'hotspot_password' ifname wlan1
```

If the target network doesn't show up in a scan (common with iPhone hotspots, which only broadcast while the Personal Hotspot screen is actively open), create the connection manually instead:
```
sudo nmcli connection add type wifi ifname wlan1 con-name "hotspot_name" ssid "hotspot_name"
sudo nmcli connection modify "hotspot_name" wifi-sec.key-mgmt wpa-psk
sudo nmcli connection modify "hotspot_name" wifi-sec.psk 'hotspot_password'
sudo nmcli connection up "hotspot_name"
```

Pi Connect becomes available to the development team automatically once wlan1 has internet access. Check status with:
```
rpi-connect status
```

Note: the D-Link dongle is currently running on its default in-kernel driver (rtl8xxxu), not the DKMS RTL8192EU driver. This works for now but hasn't been validated for long-term stability — see Key learnings for follow-up.

## Pure Data - Command Line Options

The installation launches Pd headless with explicit audio configuration flags set in the autostart command, rather than relying on Pd's saved preferences. This ensures consistent behaviour across reboots and system changes.

Current launch flags:

```
pd -nogui -alsa -alsaadd snd_rpi_hifiberry_dac8x -outchannels 8 -noadc -nomidi -r 44100 -blocksize 512 -audiobuf 25 -noprefs -open /home/mike/Documents/suuret_muinaiset/pd_codebase/main.pd
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

A pair of headphones with minijack, to test audio output directly from the Pi (DAC 5/6).

HDMI cable, screen, keyboard and mouse, for direct access to the Pi's desktop on site.

Computer or laptop for SSH/Pi Connect access when remote troubleshooting is preferred over an on-site screen.

Ethernet cable (Cat6) and a USB/Thunderbolt-to-RJ45 adapter if your laptop lacks a built-in ethernet port, for wired on-site access.

A smartphone with hotspot to connect the Pi to the wifi, allowing remote control via Pi-Connect.

Long Cat6 testing cable or cable tester, for diagnosing the underground transformer runs.

Gaffer tape, zip ties, and miscellaneous crimping tools can be helpful.