# Flash programming utility (SST39SF series)
This is a utility to program SST39SF series NOR flash ICs. It is meant to be used with an Arduino Mega or similar MCU. 
The code and pinout is heavily inspired from [slu4coder's](https://github.com/slu4coder/SST39SF010-FLASH-Programmer) and [2bitsin's](https://github.com/2bitsin/eepromutil) work, customized for my use in this project.

The pinout (copied from [2bitsin](https://github.com/2bitsin/eepromutil)):
```
  A0  - A7  : PORTA   : (Arduino pins 22, 23, 24, 25, 26, 27, 28, 29)
  A8  - A15 : PORTC   : (Arduino pins 37, 36, 35, 34, 33, 32, 31, 30)
  A16 - A18 : PORTG   : (Arduino pins 41, 40, 39)
  D0  - D7  : PORTL   : (Arduino pins 49, 48, 47, 46, 45, 44, 43, 42)
  CE#       : GND
  WE#       : PORTD.7 : (Arduino pin 38)
  OE#       : PORTB.0 : (Arduino pin 53)
```

Using the utility:
```
python flashio.py -w bin_file.bin -c COMx -b baud_rate
```
All _n_ bytes are written to the first _n_ addresses via serial communication using the credentials specified. **The default is a baud rate of 115200 on COM10**.
You must install the `PySerial` library before using.
