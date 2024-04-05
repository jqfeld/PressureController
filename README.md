# Pressure Controller


## Used components
- Arduino nano
- 16-bit ADC ADS1115 with I2C interface (used to read pressure from TPG dual-gauge
  controller)
- 12-bit DAC MCP4725 with I2C interface (used to set massflow of MFC)
- LCD-display with I2C interface
- rotary encoder (for user interaction)

## Libraries
- Wire
- LiquidCrystal_I2C
- ADS1115_WE
- Adafruit_MCP4725

## Setup
Ensure that all used devices are connected with their appropriate supply voltages.
### Arduino
Connect SDA pins of all I2C devices with pin A4 of the Arduino and all
SCL pins with A5.
The rotary encoder uses pins D2 (SW), D3 (DT) and D4 (CLK).

### Pressure reading
Connect output voltage of pressure gauge with input of the ADC (if necessary 
use appropriate voltage divider).

### Flow setting
Connect output voltage of DAC with analog voltage in of the MFC.

