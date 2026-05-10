# RV-3032-C7 Data Sheet

- Source PDF: `docs/RV-3032-C7_datasheet.pdf`
Extraction date: 2026-05-09
Page count: 2
SHA256: `0f42daa62026ca24fb551b2102e28fdc1e8638fcea8bed2fe47b2eb99c8312e2`

## Page 1

```text
DIMENSIONS
RV-3032-C7
Real-Time Clock Module with I2C-Bus
DESCRIPTION
The RV-3032-C7 is a SMD Real-Time Clock Module that incorporates 
an integrated CMOS circuit together with an XTAL. It operates under 
vacuum in a hermetically sealed ceramic package with metal lid.
APPLICATIONS
IoT
Metering
Industrial
Automotive
Health Care
Wearables, Portables
Package: Recommended Solder Pad:
0,8 max
0,15
0,9
3,2
0,4
1,5
0,5
3,2
0,9 0,9 0,9
0,5
0,8
0,8
0,4
FEATURES
Ultra low power consumption: 160 nA @ 3 V
Wide operating voltage range: 1.3 V to 5.5 V
Time accuracy:  -40°C to +85°C,     ±2.5 ppm
                          +85°C to +105°C, ±20.0 ppm
Readable 12-bit Temperature Sensor with interrupt function
User programmable Password for write protection
Automatic Backup Switchover
Trickle Charger with Charge Pump
Timer, Alarm and External Event functions
Clock output frequencies: 1 Hz to 52 MHz
I2C-bus interface: 400 kHz
100% Pb-free, RoHS-compliant
Automotive qualification according to AEC-Q200 available
BLOCK DIAGRAM
INPUT
OUTPUT
CONTROL
EVI
INT
RESET
CLKOUT
SCL
SDA
VSS
VDD
6
5
8
2
7
3
4
I2C-BUS
INTERFACE
VBACKUP
1
User EEPROM
32 Bytes of
user EEPROM
 
T-SENSOR
TEMP. COMP.
XTAL OSC.
DIVIDER
HF OSC.
39
PW 3
16 Bytes of
user RAM
EEDATA
PW 2
PW 1
EEADDR
40
4F
EECMD
PW 0
RAM
3F
EEPROM Offset
EEPROM Clkout 2
EEPROM PMU
EEPROM Clkout 1
Configuration EEPROM
with RAM mirror
C0
EEPROM TRef. 0
EEPROM TRef. 1
EEPW 0
EEPW 1
CA
EEPW 2
EEPW 3
EEPWE
POWER
MANAGEMENT
UNIT
Time Stamp THigh
Seconds to Year
TS THigh Count 
TS EVI Count 
0F
Control 1
Clock Int. Mask
Time Stamp Control 
Control 3
Time Stamp TLow
Seconds to Year
TLow Threshold
 EVI Control
THigh Threshold
27
2D
10
TS TLow Count  
Control 2
18
19
1E
1F
20
25
26
Time Stamp EVI
Temperature LSBs
Status
Timer Value 1 
Date Alarm
Timer Value 0
00
Seconds
Weekday
Hours Alarm
Minutes Alarm
Month
Hours
Minutes
Date
RAM
100th Seconds
07
08
Year
Temperature MSBsSYSTEM
CONTROL
LOGIC 
(CBh to EAh)
100th Secs. to Year
Metal lid is connected to VSS (pin #5)
All dimensions in mm typical
```

## Page 2

```text
Version 1.4/11.2022
Micro Crystal AG  Phone +41 32 655 82 82
Muehlestrasse 14  sales@microcrystal.com
CH-2540 Grenchen  www.microcrystal.com
Switzerland
  A unique part number will be generated for each product specification, i.e:
  20xxxx-MG01     1'000 pcs  (in 12 mm tape on 7" reel)
  20xxxx-MG03     3'000 pcs  (in 12 mm tape on 7" reel)
Temperature rangePackage size
TA = -40 to +85°C (Standard)
(supports extended range from +85°C
to +105°C with limitations)
Qualification
QC = Commercial Grade (Standard)
QA = Automotive Grade AEC-Q200Product type
RTC module
C7 = 3.2 x 1.5 x 0.8 mm
RV - 3032 - C7  TA  QC
TERMINATIONS AND
PROCESSING
Package Termination Processing
SON-8 Au flashed pads IPC/JEDEC J-STD-020C
260°C / 20 - 40 s
ORDERING INFORMATION
All specifications subject to change without notice.
ENVIRONMENTAL 
CHARACTERISTICS
Conditions Max. Dev.
Storage temperature range -55 to +125°C
TA Operating temperature range        -40 to +85°C 1)
Shock resistance Î”F/F 5000 g, 0.3 ms, ½ sine ±5 ppm
Vibration resistance Î”F/F 20 g / 10-2000 Hz ±5 ppm
ELECTRICAL CHARACTERISTICS 
AT 25°C
More detailed information can be found in the 
Application Manual.
Symbol Condition Min. Typ. Max Unit
Supply voltage VDD Temp. comp. 1.3 5.5 V
Current consumption  
Time keeping mode IDDO
I2C-bus inactive, 
VDD = 3 V 160 210 nA
Time accuracy Î”F/F
   -40 to +85°C ±2.5
ppm
+85 to +105°C           ±20.0
Aging first year max. Î”F/F @ 25°C ±3 ppm
PIN CONNECTIONS  
TOP VIEW
M034A1
3032
#1 #4
#5#8
Pin 1 Index
Product Date Code
Part Designation
Pin Connection
1 VBACKUP Backup Supply Voltage
2 SDA Serial Data
3 INT Interrupt Output
4 EVI Event Input
5 VSS Ground
6 VDD Power Supply Voltage
7 CLKOUT Clock Output
8 SCL Serial Clock Input
-200
-175
-150
-125
-100
-75
-50
-25
0
Î”F/F [ppm]
T[°C]
-225
-50 -30 -10 10 30 50 70 90 110
25
-40 -20 0 20 40 60 80 100
Time accuracy
Xtal 32.768 kHz
Tuning Fork Crystal
T0 = 25°C ±5°C
Î”F/F = -0.035*(T-T0)2 ppm ±10%
FREQUENCY TEMPERATURE
CHARACTERISTICS
1) Supports extended range from +85°C to +105°C with limitations.
```
