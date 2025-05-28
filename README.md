# suuret_muinaiset system

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


