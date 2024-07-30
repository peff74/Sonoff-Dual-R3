#  Sonoff Dual R3 v2

An **Arduino** program for obtaining data from a **Sonoff Dual R3 v2**  with a **bl0939** chip inside.

# ⚠️️Special Attention ⚠️️

**Do not connect AC power and the serial connection at the same time**
The GND connection of the Dual R3 is connected to the live AC wire.
Connecting serial with your PC will fry your PC and will electrocute you.
Safety and fire danger.

**DO NOT CONNECT ANY SENSORS TO THESE DEVICES!!! The GPIOs on the Dual R3 are connected to AC power!**



#  Features

 - Initializes the bl0939
 - requests data
 - reads the data 
 - does a CRC check
 - decodes the data string
 - converts the data to real values
 - Telnet for monitoring




# How to calculate Voltage, Current , Active Power
   
   More information
  - BL0939 Notes_V1.1_cn.pdf
  - BL0939_V1.2_cn.pdf
  
   Needed values
  - Vref=1.218V
   - R1=0.51kOhm
   - R2=5*390kOhm
  - RL=1mOhm
   
| Value | Calculation |
| ------ | ------ |
|  Current(A)|  = I_RMS_Reg*Vref/324004*RL|
|        |      = I_RMS_Reg / ((324004*RL)/Vref)
|   |              = I_RMS_Reg / ((324004*1)/1,218)
||              = I_RMS_Reg / 266013,14
 |  Voltage(V)|  = V_RMS_Reg*Vref*(R2+R1)/79931*R1*1000
 | |= V_RMS_Reg / ((79931*R1*1000)/(1,218*(R2+R1)))
 | |= V_RMS_Reg / ((79931*0,51*1000)/(1,218*(5*390)+0,51))
 |  |= V_RMS_Reg / 17158,92
 | Power (W)|   = WATT_Reg*Vref²*(R2+R1)/4046*RL*R1*1000
||  = WATT_Reg / ((4046*RL*R1*1000)/(Vref²*(R2+R1))
||   = WATT_Reg / ((4046*1*0,51*1000)/((1,218*1,218)*((5*390)+0,51)
||       = WATT_Reg / 713,105
| Engergy(kWh)|= CF*((1638,4*256*Vref²*(R2+R1))/(3600000*4046*RL*R1*1000))
||       = CF*((1638,4*256*(1,218*1,218)*((5*390)+0,51))/(3600000*4046*1*R0,51*1000))
||       = CF * 0,0001633819620262585
| TEMP (°C) |  = (170/448)*(TPS1/2-32)-45
|   | = ((85TSP1-5440)/448)-45
               
 # One full measurement frame from bl0939
55 3a 12 0  f0 2f 0  0  0  0  8c 61 3c bb 3  0  fc 10 0  0  0  0  b6 0  0  0  0  0  e2 1  0  7d 2  0  df
|  Byte: | 00| 01| 02| 03| 04| 05| 06| 07| 08| 09| 10| 11| 12| 13| 14| 15| 16| 17| 18| 19| 20| 21| 22| 23| 24| 25| 26| 27| 28| 29| 30| 31| 32| 33| 34|
| - |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- |- | 
|  Data:|  55| 3a| 12| 0 | f0| 2f| 0 | 0 | 0 | 0 | 8c| 61| 3c| bb| 3 | 0 | fc| 10| 0 | 0 | 0 | 0 | b6| 0 | 0 | 0 | 0 | 0 | e2| 1 | 0 | 7d| 2 | 0 | df|
  
  
 | Byte | Data |          Function |     How to calculate|
 | -----|---      |--------------      |----------|
 | 00|    0x55|           HEADER
  |01-03| 0x3a 0x12 0x00| I_FAST_RMS[A]
  |04-06| 0xf0 0x2f 0x00 |I_RMS[A] |     002ff0 --> 12272   --> /266013                --> 0,046A
  |07-09 |0x00 0x00 0x00| I_RMS[B]
  |10-12 |0x8c 0x61 0x3c| V_RMS     |    3C618C --> 3957132 --> /17159                 --> 231V
  |13-15 |0xbb 0x03 0x00 |I_FAST_RMS[B]
  |16-18 |0xfc 0x10 0x00 |WATT[A] |      0010fc --> 4348    --> /713                   --> 6,1W
  |19-21| 0x00 0x00 0x00| WATT[B]
  |22-24| 0xb6 0x00 0x00| CF_CNT[A] |    0000b6 --> 182     --> *0.000163              --> 0,0296kWh
  |25-27| 0x00 0x00 0x00| CF_CNT[B]
  |28-29| 0xe2 0x01     | TPS1[internal] | 01e2 --> 482     --> ((85*482-5440)/448)-45 --> 34,31°C
  |30|    0x00      |     Free
 | 31-32| 0x7d 0x02    |  TPS2[external]
  |33   | 0x00 |          Free
|  34    |0xdf    |       CHECKSUM


[![Hits](https://hits.seeyoufarm.com/api/count/incr/badge.svg?url=https%3A%2F%2Fgithub.com%2Fpeff74%2FSonoff-Dual-R3&count_bg=%2379C83D&title_bg=%23555555&icon=&icon_color=%23E7E7E7&title=hits&edge_flat=false)](https://hits.seeyoufarm.com)
