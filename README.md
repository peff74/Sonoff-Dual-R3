#  Sonoff Dual R3 v2

An **Arduino** program for obtaining data from a **Sonoff Dual R3 v2**  with a **bl0939** chip inside.

#  Features

 - Initializes the bl0939
 - Requests data
 - Reads the data 
 - does a CRC check
 - decodes the data string
 - converts the data to real values
 - Telnet for monitoring




## ⚠️️Special Attention ⚠️️

**Do not connect AC power and the serial connection at the same time**
The GND connection of the Dual R3 is connected to the live AC wire.
Connecting serial with your PC will fry your PC and will electrocute you.
Safety and fire danger.

**DO NOT CONNECT ANY SENSORS TO THESE DEVICES!!! The GPIOs on the Dual R3 are connected to AC power!**
