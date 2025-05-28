# suuret_muinaiset system

### <ins>Hardware/Software Requirements</ins>
This project runs on the **Teensy 4.0** microcontroller and can be updated using the Arduino framework. To program and upload code to the Teensy, you'll need to install Teensyduino, which adds Teensy support to the Arduino IDE.

+ **Download Arduino IDE:** [https://www.arduino.cc/en/software/](https://www.arduino.cc/en/software/)

+ **Download Teensyduino:** [https://www.pjrc.com/teensy/td_download.html](https://www.pjrc.com/teensy/td_download.html)

**Libraries used:**
- `Audio.h` - Teensy Audio Library (included with Teensyduino)
- `Wire.h`, `SPI.h`, `SD.h` - Standard Arduino libraries (built-in)
- `SerialFlash.h` - Serial Flash library (install via Arduino Library Manager)
- `elapsedMillis.h` - elapsedMillis library (install via Arduino Library Manager)
- `RTClib.h` - Real Time Clock library by Adafruit (install via Arduino Library Manager)
- `LedzCtrl.h` - Custom library for LEDs array control (included in project)
- `mySysCtrl.h` - Custom library for system control (included in project)

### <ins>Code</ins>
Current code updated and running on the systems can be found under *./arduino/teensy_code/teensy_code.ino*

Some variables may be modified in the teensy_code.ino lines 60-69, such as startup audio volume, startup and shutdown hours, and if the system should start with a volume knob module attached or not (only for LONG). These variables can be modified using commands (see under) during runtime, but will always be reset to default after a reboot.

### <ins>USB Commands</ins>
Different commands are available to control and get feedback from the units:

| Key | Command | Description |
|-----|---------|-------------|
| `H` | `:help` | Display this help message |
| `R` | `:report` | Generate system report |
| `B` | `:report` | Generate system reboot |
| `W` | `:wakeup` | Wake up system |
| `S` | `:sleep` | Put system to sleep |
| `P` | `:play` | Play audio |
| `!` | `:stop` | Stop audio |
| `Z` | `:replay` | Replay audio |
| `K` | `toggle` | between USB or knob (A8) volume control |
| `+` | `:volup` | Increase volume |
| `-` | `:voldown` | Decrease volume |
| |`:volume x.x`  | adjust volume, takes float between 0.0 and 1.0 |
| `>` | `:pwmup` | Increase PWM range |
| `<` | `:pwmdown` | Decrease PWM range |
| `1-4` | `:ledx` | Toggle individual LEDs |

### <ins>Diagram</ins>
A flowchart diagram can be found at >*./media/flowchart.drawio*, and runs on a free software called drawio, also available as a web version at [draw.io](www.draw.io).

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
| 9 | - | Unused |
| 10 | - | SDCARD_CS_PIN (SD card) |
| 11 | - | SDCARD_MOSI_PIN (SD card) |
| 12 | - | Unused |
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



