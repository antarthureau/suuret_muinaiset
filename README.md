# suuret_muinaiset system

### <ins>Hardware/Software Requirements</ins>
This project runs on the **Teensy 4.0** microcontroller and can be updated using the Arduino framework. To program and upload code to the Teensy, you'll need to install Teensyduino, which adds Teensy support to the Arduino IDE. The software used to open and mofidy the diagram us drawio.

+ **Download Arduino IDE:** [https://www.arduino.cc/en/software/](https://www.arduino.cc/en/software/)

+ **Download Teensyduino:** [https://www.pjrc.com/teensy/td_download.html](https://www.pjrc.com/teensy/td_download.html)

+ **Drawio**: [Download drawio](https://github.com/jgraph/drawio-desktop/releases/tag/v27.0.9) or [use online version](https://app.diagrams.net)

**Libraries used:**
- `Audio.h` - Teensy Audio Library (included with Teensyduino)
- `Wire.h`, `SPI.h`, `SD.h` - Standard Arduino libraries (built-in)
- `SerialFlash.h` - Serial Flash library (install via Arduino Library Manager)
- `elapsedMillis.h` - elapsedMillis library (install via Arduino Library Manager)
- `RTClib.h` - Real Time Clock library by Adafruit (install via Arduino Library Manager)
- `LedzCtrl.h` - Custom library for LEDs array control (included in project)
- `mySysCtrl.h` - Custom library for system control (included in project)

### <ins>Code</ins>
Current code updated and running on the systems can be found under `./arduino/teensy_code/teensy_code.ino`

Some variables may be modified in the teensy_code.ino lines 60-69, such as startup audio volume, startup and shutdown hours, and if the system should start with a volume knob module attached or not (only for LONG). These variables can be modified using commands (see under) during runtime, but will always be reset to default after a reboot.

### <ins>USB Commands</ins>
Different commands are available to control and get feedback from the units:

| Key | Command | Description |
|-----|---------|-------------|
| `H` | `:help` | Displays a help message |
| `R` | `:report` | Generate system report |
| `B` | `:reboot` | Generate system reboot |
| `W` | `:wakeup` | Wake up system |
| `S` | `:sleep` | Put system to sleep |
| `P` | `:play` | Play audio |
| `!` | `:stop` | Stop audio |
| `Z` | `:replay` | Replay audio |
| `K` | `:toggle` | between USB or knob (A8) volume control |
| `+` | `:volup` | Increase volume |
| `-` | `:voldown` | Decrease volume |
|  |`:volume x.x`  | adjust volume, takes float between 0.0 and 1.0 (ex ":volume 0.6") |
| `>` | `:pwmup` | Increase PWM range |
| `<` | `:pwmdown` | Decrease PWM range |
|  | `:ledx` | Toggle individual LEDs with an int from 0 to 15|
||`:seashell`| From LONG player only, calls for a report from seashell|
||`:small`| From LONG player only, calls for a report from small|

LONG passes USB commands further to SEASHELL and SMALL, but USB commands ran locally on SMALL or SEASHELL will not be passed to other units.

### <ins>Diagram</ins>
A flowchart diagram can be found at `./flowchart diagram.drawio` or `./flowchart diagram.pdf`, and runs on a free software called drawio, also available as a web version at [https://app.diagrams.net/](https://app.diagrams.net/).

### <ins>Pinout</ins>
|Digital Pin| Analog| Description|
|-----------|----------|------------|
| 0 | - | Must be NC, used for USB Serial |
| 1 | - | Must be NC, used for USB Serial |
| 2 | - | LED_1 |
| 3 | - | LED_2 |
| 4 | - | LED_3 |
| 5 | - | LED_4 |
| 6 | - | PWM_OUT (LED strip control) |
| 7 | - | Can be used as backup receiver for Serial2 instead of Serial3 |
| 8 | - | Can be used as backup transmitter for Serial2 instead of Serial3 |
| 9 | - | Unused / NC  |
| 10 | - | SDCARD_CS_PIN (SD card) |
| 11 | - | SDCARD_MOSI_PIN (SD card) |
| 12 | - | Unused / NC |
| 13 | - | SDCARD_SCK_PIN (SD card) |
| 14 | A0 | RX3 (receive serial on SM/SS, unused on LO) |
| 15 | A1 | TX3 (send serial on LO, unused on SM/SS) |
| 16 | - | REL_1 (relay on/off for speaker) |
| 17 | - | REL_2 (relay on/off for 220V AC Live wire to the 36V DC PSU for the amplifier) |
| 18 | - | SDA0 (RTC on LO, unused on SM/SS) |
| 19 | - | SCL0 (RTC on LO, unused on SM/SS) |
| 20 | - | LRCLK1 (audio) |
| 21 | - | BCLK1 (audio) |
| 22 | A8 | VOL_CTRL_PIN (analog volume control) |
| 23 | - | MCLK (audio) |
| 28 | - | PLAYED_ID (2, SS) |
| 30 | - | PLAYER_ID (1, SM) |
| 32 | - | PLAYER_ID (0, LO) |

### <ins>Toolkit required for maintenance</ins>
Screwdrivers to open/close the boxes and fasten terminal blocks:
+ Flat 3mm
+ Flat 6mm
+ Philips 6mm

Screwdrivers to swap boards within the boxes:
+ Philips 000 or Philips 00
+ Hex/Wrench 4
+ Hex/Wrench 4.5mm

Multimeter to test voltage and continuity

Wago connectors and crocodile cables to test temporary connections

A pair of headphones with minijack to test audio output on teensy

Computer with USB-A connector and Arduino + Teensyduino installed to upload new code

Any serial monitor software over USB to monitor the systems
