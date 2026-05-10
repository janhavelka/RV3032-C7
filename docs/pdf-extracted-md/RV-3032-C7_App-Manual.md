# RV-3032-C7 Application Manual

- Source PDF: `docs/RV-3032-C7_App-Manual.pdf`
Extraction date: 2026-05-09
Page count: 154
SHA256: `f07894dcc414b58a889f4e200cfdb53c6f63e22951d10b7febc6698df7aba504`

## Page 1

```text
RV-3032-C7 
Application Manual 
 
May 2023 1/154 Rev. 1.3 
 
 
 
Application 
Manual 
 
RV-3032-C7 
 
DTCXO Temp. Compensated 
Real-Time Clock Module 
with I2C-Bus Interface
```

## Page 2

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 2/154 Rev. 1.3 
TABLE OF CONTENTS 
 
1. OVERVIEW ....................................................................................................................................................... 7 
1.1. GENERAL DESCRIPTION ......................................................................................................................... 8 
1.2. APPLICATIONS ......................................................................................................................................... 8 
1.3. ORDERING INFORMATION ...................................................................................................................... 8 
2. BLOCK DIAGRAM ............................................................................................................................................ 9 
2.1. PINOUT .................................................................................................................................................... 10 
2.2. PIN DESCRIPTION .................................................................................................................................. 10 
2.3. FUNCTIONAL DESCRIPTION ................................................................................................................. 11 
2.4. DEVICE PROTECTION DIAGRAM ......................................................................................................... 12 
3. REGISTER ORGANIZATION ......................................................................................................................... 13 
3.1. REGISTER CONVENTIONS .................................................................................................................... 13 
3.2. REGISTER OVERVIEW ........................................................................................................................... 14 
3.3. CLOCK REGISTERS ............................................................................................................................... 17 
3.4. CALENDAR REGISTERS ........................................................................................................................ 18 
3.5. ALARM REGISTERS ............................................................................................................................... 20 
3.6. PERIODIC COUNTDOWN TIMER CONTROL REGISTERS .................................................................. 21 
3.7. STATUS REGISTER ................................................................................................................................ 22 
3.8. TEMPERATURE REGISTERS................................................................................................................. 23 
3.9. CONTROL REGISTERS .......................................................................................................................... 25 
3.10. TIME STAMP CONTROL REGISTER ..................................................................................................... 28 
3.11. CLOCK INTERRUPT MASK REGISTER ................................................................................................ 29 
3.12. EVI CONTROL REGISTER ...................................................................................................................... 30 
3.13. TEMPERATURE THRESHOLDS REGISTERS ....................................................................................... 31 
3.14. TIME STAMP TLOW REGISTERS .......................................................................................................... 32 
3.15. TIME STAMP THIGH REGISTERS ......................................................................................................... 35 
3.16. TIME STAMP EVI REGISTERS ............................................................................................................... 38 
3.17. PASSWORD REGISTERS ....................................................................................................................... 42 
3.18. EEPROM MEMORY CONTROL REGISTERS ........................................................................................ 43 
3.19. RAM REGISTERS .................................................................................................................................... 44 
3.20. CONFIGURATION EEPROM WITH RAM MIRROR REGISTERS .......................................................... 45 
3.20.1. EEPROM PMU REGISTER .............................................................................................................. 45 
3.20.2. EEPROM OFFSET REGISTER ........................................................................................................ 46 
3.20.3. EEPROM CLKOUT REGISTERS ..................................................................................................... 47 
3.20.4. EEPROM TEMPERATURE REFERENCE REGISTERS ................................................................. 49 
3.20.5. EEPROM PASSWORD REGISTERS ............................................................................................... 50 
3.20.6. EEPROM PASSWORD ENABLE REGISTER .................................................................................. 51 
3.21. USER EEPROM ....................................................................................................................................... 51 
3.22. REGISTER RESET VALUES SUMMARY ............................................................................................... 52
```

## Page 3

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 3/154 Rev. 1.3 
4. DETAILED FUNCTIONAL DESCRIPTION .................................................................................................... 57 
4.1. POWER ON RESET (POR) ...................................................................................................................... 57 
4.2. AUTOMATIC BACKUP SWITCHOVER FUNCTION............................................................................... 58 
4.2.1. SWITCHOVER DISABLED ............................................................................................................... 59 
4.2.2. DIRECT SWITCHING MODE (DSM) ................................................................................................ 59 
4.2.3. LEVEL SWITCHING MODE (LSM) ................................................................................................... 60 
4.3. TRICKLE CHARGER WITH CHARGE PUMP ......................................................................................... 61 
4.4. PROGRAMMABLE CLOCK OUTPUT .................................................................................................... 61 
4.4.1. XTAL CLKOUT FREQUENCY SELECTION..................................................................................... 62 
4.4.2. HF CLKOUT FREQUENCY SELECTION ......................................................................................... 62 
4.4.3. CLKOUT FREQUENCY TRANSITIONS ........................................................................................... 63 
4.4.4. NORMAL CLOCK OUTPUT .............................................................................................................. 63 
4.4.5. INTERRUPT CONTROLLED CLOCK OUTPUT ............................................................................... 63 
4.4.6. INTERRUPT DELAY AFTER CLKOUT ON ...................................................................................... 64 
4.4.7. CLKOUT OFF DELAY AFTER I2C STOP ......................................................................................... 64 
4.4.8. CLOCK OUTPUT SCHEME .............................................................................................................. 65 
4.5. SETTING AND READING THE TIME ...................................................................................................... 66 
4.5.1. SETTING THE TIME ......................................................................................................................... 67 
4.5.2. READING THE TIME ........................................................................................................................ 67 
4.6. EEPROM READ/WRITE .......................................................................................................................... 68 
4.6.1. POR REFRESH (ALL CONFIGURATION EEPROM ? RAM) ......................................................... 68 
4.6.2. AUTOMATIC REFRESH (ALL CONFIGURATION EEPROM ? RAM) ........................................... 68 
4.6.3. UPDATE (ALL CONFIGURATION RAM ? EEPROM) .................................................................... 68 
4.6.4. REFRESH (ALL CONFIGURATION EEPROM ? RAM) .................................................................. 68 
4.6.5. WRITE TO ONE EEPROM BYTE (EEDATA (RAM) ? EEPROM) .................................................. 69 
4.6.6. READ ONE EEPROM BYTE (EEPROM ? EEDATA (RAM)) .......................................................... 69 
4.6.7. EEBUSY BIT ..................................................................................................................................... 70 
4.6.8. EEF FLAG ......................................................................................................................................... 71 
4.6.9. EEPROM READ/WRITE CONDITIONS ........................................................................................... 71 
4.6.10. USE OF THE CONFIGURATION REGISTERS ............................................................................... 72 
4.7. INTERRUPT OUTPUT ............................................................................................................................. 73 
4.7.1. SERVICING INTERRUPTS .............................................................................................................. 73 
4.7.2. INTERRUPT SCHEME ..................................................................................................................... 74 
4.8. PERIODIC COUNTDOWN TIMER INTERRUPT FUNCTION ................................................................. 77 
4.8.1. PERIODIC COUNTDOWN TIMER DIAGRAM.................................................................................. 77 
4.8.2. USE OF THE PERIODIC COUNTDOWN TIMER INTERRUPT ....................................................... 78 
4.8.3. FIRST PERIOD DURATION ............................................................................................................. 80 
4.9. PERIODIC TIME UPDATE INTERRUPT FUNCTION ............................................................................. 81 
4.9.1. PERIODIC TIME UPDATE DIAGRAM .............................................................................................. 81
```

## Page 4

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 4/154 Rev. 1.3 
4.9.2. USE OF THE PERIODIC TIME UPDATE INTERRUPT ................................................................... 82 
4.10. ALARM INTERRUPT FUNCTION............................................................................................................ 83 
4.10.1. ALARM DIAGRAM ............................................................................................................................ 83 
4.10.2. USE OF THE ALARM INTERRUPT .................................................................................................. 84 
4.11. EXTERNAL EVENT INTERRUPT FUNCTION ........................................................................................ 85 
4.11.1. EXTERNAL EVENT DIAGRAM ........................................................................................................ 86 
4.11.2. USE OF THE EXTERNAL EVENT INTERRUPT .............................................................................. 87 
4.11.3. EDGE DETECTION (ET = 00) .......................................................................................................... 88 
4.11.4. LEVEL DETECTION WITH FILTERING (ET ≠ 00) ........................................................................... 88 
4.12. TEMPERATURE LOW INTERRUPT FUNCTION .................................................................................... 89 
4.12.1. TEMPERATURE LOW DIAGRAM .................................................................................................... 90 
4.12.2. USE OF THE TEMPERATURE LOW INTERRUPT ......................................................................... 91 
4.13. TEMPERATURE HIGH INTERRUPT FUNCTION ................................................................................... 92 
4.13.1. TEMPERATURE HIGH DIAGRAM ................................................................................................... 93 
4.13.2. USE OF THE TEMPERATURE HIGH INTERRUPT ......................................................................... 94 
4.14. AUTOMATIC BACKUP SWITCHOVER INTERRUPT FUNCTION ......................................................... 95 
4.14.1. AUTOMATIC BACKUP SWITCHOVER DIAGRAM .......................................................................... 95 
4.14.2. USE OF THE AUTOMATIC BACKUP SWITCHOVER INTERRUPT ............................................... 96 
4.15. POWER ON RESET INTERRUPT FUNCTION ....................................................................................... 97 
4.15.1. POWER ON RESET DIAGRAM ....................................................................................................... 97 
4.15.2. USE OF THE POWER ON RESET INTERRUPT ............................................................................. 98 
4.16. VOLTAGE LOW INTERRUPT FUNCTION .............................................................................................. 99 
4.16.1. VOLTAGE LOW DIAGRAM .............................................................................................................. 99 
4.16.2. USE OF THE VOLTAGE LOW INTERRUPT .................................................................................. 100 
4.17. TIME STAMP EVI FUNCTION ............................................................................................................... 101 
4.17.1. TIME STAMP EVI SCHEME ........................................................................................................... 102 
4.18. TIME STAMP TLOW FUNCTION .......................................................................................................... 103 
4.18.1. TIME STAMP TLOW SCHEME ...................................................................................................... 104 
4.19. TIME STAMP THIGH FUNCTION .......................................................................................................... 105 
4.19.1. TIME STAMP THIGH SCHEME ...................................................................................................... 106 
4.20. TEMPERATURE REFERENCE ADJUSTMENT ................................................................................... 107 
4.20.1. TREF VALUE DETERMINATION ................................................................................................... 107 
4.21. TIME SYNCHRONIZATION ................................................................................................................... 109 
4.21.1. STOP BIT FUNCTION .................................................................................................................... 109 
4.21.2. WRITING TO SECONDS REGISTER ............................................................................................ 111 
4.21.3. ESYN BIT FUNCTION .................................................................................................................... 112 
4.22. USER PROGRAMMABLE PASSWORD ............................................................................................... 114 
4.22.1. ENABLE/DISABLE WRITE PROTECTION..................................................................................... 114 
4.22.2. CHANGING PASSWORD ............................................................................................................... 115
```

## Page 5

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 5/154 Rev. 1.3 
4.22.3. FLOWCHART .................................................................................................................................. 116 
5. TEMPERATURE COMPENSATION ............................................................................................................. 117 
5.1. XTAL MODE FREQUENCIES................................................................................................................ 117 
5.2. COMPENSATION VALUES ................................................................................................................... 117 
5.3. AGING CORRECTION ........................................................................................................................... 118 
5.3.1. OFFSET VALUE DETERMINATION .............................................................................................. 118 
5.3.2. MEASURING TIME ACCURACY AT CLKOUT PIN ....................................................................... 119 
5.4. CLOCKING SCHEME ............................................................................................................................ 120 
6. I2C INTERFACE ............................................................................................................................................ 121 
6.1. BIT TRANSFER ..................................................................................................................................... 121 
6.2. START AND STOP CONDITIONS ........................................................................................................ 121 
6.3. DATA VALID .......................................................................................................................................... 122 
6.4. SYSTEM CONFIGURATION .................................................................................................................. 122 
6.5. ACKNOWLEDGE ................................................................................................................................... 123 
6.6. SLAVE ADDRESS ................................................................................................................................. 124 
6.7. WRITE OPERATION .............................................................................................................................. 124 
6.8. READ OPERATION AT SPECIFIC ADDRESS ..................................................................................... 125 
6.9. READ OPERATION ............................................................................................................................... 125 
6.10. I2C-BUS IN SWITCHOVER CONDITION ............................................................................................... 126 
7. ELECTRICAL SPECIFICATIONS ................................................................................................................. 127 
7.1. ABSOLUTE MAXIMUM RATINGS ........................................................................................................ 127 
7.2. OPERATING PARAMETERS ................................................................................................................ 128 
7.2.1. TEMPERATURE COMPENSATION AND CURRENT CONSUMPTION ....................................... 130 
7.2.2. TYPICAL CHARACTERISTICS ...................................................................................................... 131 
7.3. XTAL OSCILLATOR PARAMETERS .................................................................................................... 132 
7.3.1. TIME ACCURACY VS. TEMPERATURE CHARACTERISTICS .................................................... 133 
7.3.2. TIME ACCURACY 1 HZ EXAMPLES ............................................................................................. 134 
7.3.3. TEMPERATURE SENSOR ACCURACY EXAMPLE ..................................................................... 135 
7.4. POWER ON AC ELECTRICAL CHARACTERISTICS .......................................................................... 136 
7.5. BACKUP AND RECOVERY AC ELECTRICAL CHARACTERISTICS  ................................................ 138 
7.6. I2C-BUS CHARACTERISTICS ............................................................................................................... 139 
8. TYPICAL APPLICATION CIRCUITS ............................................................................................................ 140 
8.1. NO BACKUP SOURCE / EVENT INPUT NOT USED ........................................................................... 140 
8.2. NON-RECHARGEABLE BACKUP SOURCE / EVENT INPUT USED (ACTIVE HIGH)  ...................... 141 
8.3. RECHARGEABLE BACKUP SOURCE / EVENT INPUT USED (ACTIVE LOW)  ................................ 142 
8.4. CERACHARGE™ BACKUP BATTERY / EVENT INPUT NOT USED ................................................. 143 
8.5. NO BACKUP SOURCE / EVENT INPUT USED (“WAKE-UP” & “POWER SWITCH”) ...................... 144 
9. PACKAGE ..................................................................................................................................................... 145 
9.1. DIMENSIONS AND SOLDER PAD LAYOUT ........................................................................................ 145
```

## Page 6

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 6/154 Rev. 1.3 
9.1.1. RECOMMENDED THERMAL RELIEF ........................................................................................... 145 
9.2. MARKING AND PIN #1 INDEX .............................................................................................................. 146 
10. MATERIAL COMPOSITION DECLARATION & ENVIRONMENTAL INFORMATION  ............................... 147 
10.1. HOMOGENOUS MATERIAL COMPOSITION DECLARATION ........................................................... 147 
10.2. MATERIAL ANALYSIS & TEST RESULTS .......................................................................................... 148 
10.3. RECYCLING MATERIAL INFORMATION ............................................................................................ 149 
10.4. ENVIRONMENTAL PROPERTIES & ABSOLUTE MAXIMUM RATINGS ........................................... 150 
11. SOLDERING INFORMATION ....................................................................................................................... 151 
12. HANDLING PRECAUTIONS FOR MODULES WITH EMBEDDED CRYSTALS  ........................................ 152 
13. PACKING & SHIPPING INFORMATION ...................................................................................................... 153 
14. COMPLIANCE INFORMATION .................................................................................................................... 154 
15. DOCUMENT REVISION HISTORY ............................................................................................................... 154
```

## Page 7

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 7/154 Rev. 1.3 
RV-3032-C7 
 
DTCXO Temp. Compensated Real-Time Clock (RTC) Module with I2C-Bus Interface 
 
1. OVERVIEW 
? RTC module with built-in 32.768 kHz “Tuning Fork” crystal oscillator and HF oscillator 
? Counters for hundredths of seconds, seconds, minutes, hours, weekday, date, month and year 
o Automatic leap year correction: 2000 to 2099 
? Factory calibrated temperature compensation 
? Very High Time Accuracy 
o ±2.5 ppm from -40 to +85°C 
o ±20 ppm from +85 to +105°C 
o Aging compensation with Offset value 
? Interrupt functions (1) 
o Periodic Countdown Timer 
o Periodic Time Update (seconds, minutes) 
o Alarm for date, hours and minutes settings 
o External Event with Time Stamp 
o Temperature Low with Time Stamp 
o Temperature High with Time Stamp 
o Automatic Backup Switchover 
o Power On Reset (POR) 
o Voltage Low Detection: typical 1.2 V (sampling 1 Hz) 
? 16 Bytes of User RAM and 32 Bytes of User EEPROM 
? Configuration registers stored in EEPROM and mirrored in RAM 
? User programmable password for write protection 
? I2C-bus interface (up to 400 kHz) 
o 7-bit slave address = 1010001b (51h): read A3h, write A2h 
? Programmable Clock Output for peripheral devices 
o XTAL mode: 32.768 kHz, 1024 Hz, 64 Hz, 1 Hz 
o HF mode:   8192 Hz to 67.109 MHz in 8192 Hz steps 
o Synchronized CLKOUT enable/disable and oscillator change 
o Selectable enable via Interrupt functions 
o Selectable INTÌ…Ì…Ì…Ì…Ì… delay for MCU wake up 
o Selectable CLKOUT-OFF delay for MCU sleep mode command 
? Digital Thermometer (sampling 1 Hz) 
o ±3°C from -40 to +85°C 
o ±7°C from +85 to +105°C 
o Readable and adjustable 12-bit temperature sensor: resolution 0.0625°C/step 
? Trickle Charger with Charge Pump for VBACKUP ≥ VDD and 1.75 V for TDK's CeraCharge™ 
? Wide Timekeeping voltage range: 1.3 to 5.5 V (including temperature sensing and compensation) 
? Wide interface operating voltage: 1.4 to 5.5 V 
? Very low current consumption: 160 nA (VDD = 3.0 V, TA = 25°C) 
? Operating temperature range: -40 to +85°C 
(supports extended range from +85°C to +105°C with limitations) 
? Ultra small and compact C7 package size (3.2 x 1.5 x 0.8 mm), RoHS-compliant and 100% lead-free 
? Automotive qualification according to AEC-Q200 available 
 
(1) The interrupt output pin INTÌ…Ì…Ì…Ì…Ì… also works in VBACKUP Power state (except for the External Event function which 
is disabled).
```

## Page 8

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 8/154 Rev. 1.3 
1.1. GENERAL DESCRIPTION 
The RV-3032-C7 is a highly accurate real -time clock/calendar module due to its built -in Thermometer and Digital 
Temperature Compensation circuitry (DTCXO). The Temperature Compensation circuitry is factory calibrated and 
results in highest time  accuracy of ±2.5 ppm across the temperature range from -40 to +85°C and a time accuracy 
of ±20 ppm for the extended range from +85°C to +105°C , and additionally offers  a non -volatile aging offset 
correction. The RV-3032-C7 has the smallest package and the  lowest current consumption among all temperature 
compensated RTC modules. Due to its special architecture the RV-3032-C7 provides a very low current consumption 
of 160 nA. 
The RV-3032-C7 CMOS real -time clock/calendar module includes automatic backup switching circuitry,  a trickle 
charger with charge pump, and offers full RTC functionality with programmable counters, alarm, selectable interrupt, 
and clock output capabilities for frequencies from 1 Hz to 67 MHz. The internal EEPROM memory hosts all 
configuration settings and allows for additional user memory. Addresses and data are transmitted via an I 2C-bus 
interface for communication with a host controller. The Address Pointer is incremented automatically after each 
written or read data byte. 
 
 
1.2. APPLICATIONS 
The RV-3032-C7 RTC module combines key functions with outs tanding performance in an ultra-small ceramic 
package: 
? Factory calibrated Temperature Compensation with temperature measuring every second  
? Ultra-Low Power consumption 
? Smallest RTC module (embedded XTAL) in an ultra-small 3.2 x 1.5 x 0.8 mm lead-free ceramic package 
 
These unique features make this product perfectly suitable for many applications:  
? Communication:  IoT / Wearables / Wireless Sensors and Tags / Handsets 
? Automotive:     M2M / Navigation & Tracking Systems / Dashboard / Tachometers / Engine Controller  
            Car Audio & Entertainment Systems 
? Metering:     E-Meter / Heating Counter / Smart Meters / PV Converter / Utility metering 
? Outdoor:      ATM & POS systems / Surveillance & Safety systems / Ticketing Systems 
? Medical:      Glucose Meter / Health Monitoring Systems 
? Safety:       Security & Camera Systems / Door Lock & Access Control / Tamper Detection 
? Consumer:    Gambling Machines / TV & Set Top Boxes / White Goods 
? Automation:    PLC / Data Logger / Home & Factory Automation / Industrial and Consumer Electronics  
 
 
1.3. ORDERING INFORMATION 
Example:    RV-3032-C7    TA    QC 
 
Code Operating temperature range 
TA (Standard)     -40 to +85°C   1) 
1) Supports extended range from +85°C to +105°C with limitations. 
 
Code Qualification 
QC (Standard) Commercial Grade 
QA Automotive Grade AEC-Q200
```

## Page 9

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 9/154 Rev. 1.3 
2. BLOCK DIAGRAM 
 
SYSTEM
CONTROL
LOGIC
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
10
EEPROM Offset
EEPROM Clkout 2
EEPROM PMU
EEPROM Clkout 1
Configuration EEPROM
with RAM mirror
TS TLow Count  
C0
EEPROM TRef. 0Control 2
T-SENSOR
TEMP. COMP.
XTAL OSC.
DIVIDER
HF OSC.
18
19
1E
1F
20
25
26
Time Stamp EVI 
100th Secs. to Year
Temperature LSBs
Status
Timer Value 1
Date Alarm
Timer Value 0 
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
Temperature MSBs
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
User EEPROM
32 Bytes of
user EEPROM
(CBh to EAh)
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
```

## Page 10

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 10/154 Rev. 1.3 
2.1. PINOUT 
 
 
2.2. PIN DESCRIPTION 
Symbol Pin # Description 
VBACKUP 1 Backup Supply Voltage. When the backup switchover function is not needed, VBACKUP must be tied 
to VSS with a 10 kΩ resistor. 
SDA 2 I2C Serial Data Input-Output; open-drain; requires pull-up resistor. In VBACKUP Power state, the 
SDA pin is disabled (high impedance). 
INTÌ…Ì…Ì…Ì…Ì… 3 
Interrupt Output; open-drain; active LOW; requires pull-up resistor; used to output Periodic 
Countdown Timer, Periodic Time Update, Alarm, Temperature Low, Temperature High, External 
Event, Voltage Low, Automatic Backup Switchover and Power On Reset Interrupt signals. Interrupt 
output also works in VBACKUP Power state (except for the External Event function which is 
disabled). 
EVI 4 
External Event Input; used for interrupt generation, interrupt driven clock output and time stamp 
function. In VBACKUP Power state, the External Event function is disabled. This pin must not be 
left floating. Do not tie this pin to VBACKUP (ESD diode between EVI and VDD). 
VSS 5 Ground. 
VDD 6 Power Supply Voltage. 
CLKOUT 7 
Clock Output; push-pull; Normal and Interrupt driven clock output can be activated concurrently. 
1. Normal clock output is controlled by the NCLKE bit (EEPROM C0h). When NCLKE is set 
to 0 (default), the square wave output is enabled on the CLKOUT pin. 
When NCLKE bit is set to 1, the CLKOUT pin is LOW, if not enabled by the interrupt 
driven clock output (CLKF = 0). 
2. Interrupt driven clock output is controlled by an interrupt event. When CLKIE bit (address 
11h) is set to 1 the occurrence of the interrupt selected in the Clock Interrupt Mask 
Register (address 14h) allows the square wave output on the CLKOUT pin (for waking up 
the MCU). Writing 0 to CLKIE will disable new interrupts from driving square wave on 
CLKOUT. When CLKF flag is cleared, the CLKOUT pin is LOW. 
? An Interrupt Delay after CLKOUT on can be enabled with bit INTDE (address 
14h) (for waking up the MCU). 
? A CLKOUT switch off delay after I2C STOP can be selected and enabled by 
bits CLKD and CLKDE (registers 14h and 15h) (if the MCU wants to put itself 
into sleep mode). 
When OS bit is set to 0 (EEPROM C3h) and depending of the settings in the FD field (EEPROM 
C3h) the CLKOUT pin can drive the square wave of 32.768 kHz, 1024 Hz, 64 Hz or 1 Hz. 
When OS bit is set to 1 (EEPROM C3h) and depending of the settings in the HFD field (EEPROM 
C2h and C3h) the CLKOUT pin can drive the square wave of a frequency between 8192 Hz to 
67.109 MHz in 8192 Hz steps. 
In VBACKUP Power state, the CLKOUT pin is LOW. 
If this pin is not used it can be left floating; do not connect to a Supply Voltage or GND, as this is a 
digital push-pull output. 
SCL 8 I2C Serial Clock Input; requires pull-up resistor. In VBACKUP Power state, the SCL pin is disabled. 
 
  
 
RV-3032-C7 Package: (top view) 
 
#1 #4
#5#8
3032
 
 
  
 #1    VBACKUP  
 #2    SDA  
 #3    INT  
 #4    EVI  
 #5    VSS  
 #6    VDD  
 #7    CLKOUT  
 #8    SCL
```

## Page 11

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 11/154 Rev. 1.3 
2.3. FUNCTIONAL DESCRIPTION 
The RV-3032-C7 is a high accurate, ultra -low power CMOS based Real -Time-Clock Module that include a  32.768 
kHz Crystal and an HF oscillator. The Xtal 32.768 kHz clock itself and the frequencies from the HF oscillator are not 
temperature compensated. The very high Time Accuracy and stability of ±2.5 ppm over the temperature range from 
-40°C to +85°C and of ±20 ppm for the extended range from +85°C t o +105°C is achieved by the built -in Digital 
Temperature Compensation circuitry (DTCXO). The factory calibrated correction values are located in the EEPROM 
and are not accessible for the user. Additionally, there is an EEPROM Offset Register customer use f or aging 
correction. 
 
The RV-3032-C7 includes an Automatic Backup switchover function and a Trickle Charger with Charge Pump . The 
interrupt output pin INT is also working  in VBACKUP Power state  (except for the External Event function which is 
disabled). The clock output on CLKOUT pin can be enabled normally via command over I 2C interface or can be 
interrupt driven. The configuration registers are stored permanently in EEPROM and mirrored in RAM in order that 
the RTC module is still configured correctly even  after power down. For safety against inadvertent overwriting, the 
time, control and configuration registers can be protected by a User Programmable Password.  
 
The RV-3032-C7 provides standard Clock & Calendar function including 100th seconds, seconds, minutes, hours (24 
hour mode ), weekday, date, month, year  (with leap year correction) and interrupt functions for  the Periodic 
Countdown Timer, Periodic Time Update, Alarm, Temperature Low, Temperature High, External Event, Voltage Low, 
Automatic Backup Switchover and Power On Reset Interrupt signals. All registers are accessible via I2C-bus (2-wire 
Interface). 
Beside the standard RTC functions, it contains an integrated 12-bit Temperature Sensor with a readable Temperature 
Value in °C with a resolution of 0.0625°C/step and an adjustable Temperature Reference value. It also includes Time 
Stamp functions for the External Event, Temperature Low, and Temperature High Interrupt functions. The Interrupt 
and Time Stamp functions are also working in VBACK UP Power state (except for the External Event function which 
is disabled). The module also provides 16 Bytes of User RAM and 32 Bytes of User Memory EEPROM. Another 
RAM Byte can be used as User RAM when the Alarm function is not needed (Minutes Alarm, register 08h), another 
Byte if the Periodic Countdown Timer is not used (Timer Value 0 , register 0Bh) and  2 further Bytes when the 
Temperature Thresholds are not needed (Thresholds TLT and THT, registers 16h and 17h). 
 
The RAM registers are accessed by se lecting a register address and then performing read or write operations. 
Multiple reads or writes may be executed in a single access, with the address automatically incrementing after each 
byte. When address is automatically incremented, wrap around occurs from address FFh to address 00h (see figure 
below). All registers are designed as addressable 8 -bit registers despite the fact that not all registers and bits are 
implemented. 
 
Handling RAM address registers: 
 
   
00h
01h
02h
03h
:
FDh
FEh
FFh
Address
wrap around
auto-
increment
```

## Page 12

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 12/154 Rev. 1.3 
2.4. DEVICE PROTECTION DIAGRAM 
 
VSS
SDA
SCLVBACKUP
CLKOUT
EVI
2
1
3
4 5
6
7
8
INT VDD
```

## Page 13

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 13/154 Rev. 1.3 
3. REGISTER ORGANIZATION 
? RAM Registers at addresses 00h to 4Fh are accessed by selecting a register address and then performing 
read or write operations. Multiple reads or writes may be executed in a single access, with the address 
automatically incrementing after each byte. 
? The Configuration Registers at addresses C0h to CAh are memorized in EEPROM and mirrored in RAM.  
For the RAM mirror, multiple reads or writes may be executed in a single access, with the address 
automatically incrementing after each byte. 
? There are 32 Bytes of non-volatile user memory EEPROM at addresses CBh to EAh for general use. 
 
The tables in section REGISTER OVERVIEW summarize the function of each register. 
 
 
3.1. REGISTER CONVENTIONS 
The conventions in this table serve as a key for the register overview and individual register diagrams:  
Convention 
(Conv.) Description 
R Read only. Writing to this register has no effect. 
W Write only. Returns 0 when read. 
R/WP Read: Always readable. Write: Can be write-protected by password. 
WP Write only. Returns 0 when read. Can be write-protected by password. 
*WP EEPW registers: Can be write-protected by password. 
RAM mirror is Write only. Returns 0 when read. EEPROM can be READ when unlocked. 
Prot. Protected. Writing to this register has no effect.
```

## Page 14

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 14/154 Rev. 1.3 
3.2. REGISTER OVERVIEW 
After reset, all registers are set according to Table in section REGISTER RESET VALUES SUMMARY. 
 
Register Definitions; RAM, Address 00h to 25h: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
00h 100th Seconds R 80 40 20 10 8 4 2 1 
01h Seconds R/WP ○ 40 20 10 8 4 2 1 
02h Minutes R/WP ○ 40 20 10 8 4 2 1 
03h Hours R/WP ○ ○ 20 10 8 4 2 1 
04h Weekday R/WP ○ ○ ○ ○ ○ 4 2 1 
05h Date R/WP ○ ○ 20 10 8 4 2 1 
06h Month R/WP ○ ○ ○ 10 8 4 2 1 
07h Year R/WP 80 40 20 10 8 4 2 1 
08h Minutes Alarm R/WP AE_M 40 20 10 8 4 2 1 
09h Hours Alarm R/WP AE_H ○ 20 10 8 4 2 1 
0Ah Date Alarm R/WP AE_D ○ 20 10 8 4 2 1 
0Bh Timer Value 0 R/WP 128 64 32 16 8 4 2 1 
0Ch Timer Value 1 R/WP ○ ○ ○ ○ 2048 1024 512 256 
0Dh Status R/WP THF TLF UF TF AF EVF PORF VLF 
0Eh Temperature LSBs R/WP TEMP [3:0] EEF EEbusy CLKF BSF 
0Fh Temperature MSBs R TEMP [11:4] 
10h Control 1 R/WP - - GP0 USEL TE EERD TD 
11h Control 2 R/WP - CLKIE UIE TIE AIE EIE GP1 STOP 
12h Control 3 R/WP - - - BSIE THE TLE THIE TLIE 
13h Time Stamp Contr. R/WP - - EVR THR TLR EVOW THOW TLOW 
14h Clock Int. Mask R/WP CLKD INTDE CEIE CAIE CTIE CUIE CTHIE CTLIE 
15h EVI Control R/WP CLKDE EHL ET ○ ○ ○ ESYN 
16h TLow Threshold R/WP TLT 
17h THigh Threshold R/WP THT 
18h TS TLow Count R 128 64 32 16 8 4 2 1 
19h TS TLow Seconds R ○ 40 20 10 8 4 2 1 
1Ah TS TLow Minutes R ○ 40 20 10 8 4 2 1 
1Bh TS TLow Hours R ○ ○ 20 10 8 4 2 1 
1Ch TS TLow Date R ○ ○ 20 10 8 4 2 1 
1Dh TS TLow Month R ○ ○ ○ 10 8 4 2 1 
1Eh TS TLow Year R 80 40 20 10 8 4 2 1 
1Fh TS THigh Count R 128 64 32 16 8 4 2 1 
20h TS THigh Seconds R ○ 40 20 10 8 4 2 1 
21h TS THigh Minutes R ○ 40 20 10 8 4 2 1 
22h TS THigh Hours R ○ ○ 20 10 8 4 2 1 
23h TS THigh Date R ○ ○ 20 10 8 4 2 1 
24h TS THigh Month R ○ ○ ○ 10 8 4 2 1 
25h TS THigh Year R 80 40 20 10 8 4 2 1 
○ Read only. Always 0. 
- Bit not implemented. Will return a 0 when read.
```

## Page 15

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 15/154 Rev. 1.3 
Register Definitions; RAM, Address 26h to FFh: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
26h TS EVI Count R 128 64 32 16 8 4 2 1 
27h TS EVI 100th Secs. R 80 40 20 10 8 4 2 1 
28h TS EVI Seconds R ○ 40 20 10 8 4 2 1 
29h TS EVI Minutes R ○ 40 20 10 8 4 2 1 
2Ah TS EVI Hours R ○ ○ 20 10 8 4 2 1 
2Bh TS EVI Date R ○ ○ 20 10 8 4 2 1 
2Ch TS EVI Month R ○ ○ ○ 10 8 4 2 1 
2Dh TS EVI Year R 80 40 20 10 8 4 2 1 
2Eh to 38h RESERVED Prot. RESERVED 
39h Password 0 W PW [7:0] 
3Ah Password 1 W PW [15:8] 
3Bh Password 2 W PW [23:16] 
3Ch Password 3 W PW [31:24] 
3Dh EE Address R/WP EEADDR 
3Eh EE Data R/WP EEDATA 
3Fh EE Command WP EECMD 
40h to 4Fh User RAM 
(16 Bytes) R/WP 16 Bytes of User RAM 
50h to BFh RESERVED Prot. RESERVED 
CBh to FFh RESERVED Prot. RESERVED 
○ Read only. Always 0.
```

## Page 16

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 16/154 Rev. 1.3 
Register Definitions; Configuration EEPROM with RAM mirror, Address C0h to CAh: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C0h EEPROM PMU R/WP - NCLKE BSM TCR TCM 
C1h EEPROM Offset R/WP PORIE VLIE OFFSET 
C2h EEPROM 
Clkout 1 R/WP HFD [7:0] 
C3h EEPROM 
Clkout 2 R/WP OS FD HFD [12:8] 
C4h EEPROM 
TReference 0 R/WP TREF [7:0] 
C5h EEPROM 
TReference 1 R/WP TREF [15:8] 
C6h EEPROM 
Password 0 *WP EEPW [7:0] 
C7h EEPROM 
Password 1 *WP EEPW [15:8] 
C8h EEPROM 
Password 2 *WP EEPW [23:16] 
C9h EEPROM 
Password 3 *WP EEPW [31:24] 
CAh EEPROM PW 
Enable WP EEPWE 
- Bit not implemented. Will return a 0 when read. 
*WP: The EEPW registers can be write-protected by password. 
         RAM mirror is Write only. Returns 0 when read. EEPROM can be READ when unlocked. 
 
 
Register Definitions; User EEPROM, Address CBh to EAh: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
CBh to EAh User EEPROM 
(32 Bytes) R/WP 32 Bytes of non-volatile User EEPROM
```

## Page 17

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 17/154 Rev. 1.3 
3.3. CLOCK REGISTERS 
00h - 100th Seconds 
This register holds the count of hundredths of seconds, in two binary coded decimal (BCD) digits. Values will be from 
00 to 99. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
00h 100th Seconds R 80 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 100th Seconds 00 to 99 
Holds the count of hundredths of seconds, coded in BCD format. 
When STOP bit is set to 1 or when writing to the Seconds register or when 
ESYN bit is 1 in case of an External Event detection on EVI pin the 100th 
Seconds register value is cleared to 00 (see TIME SYNCHRONIZATION). 
 
 
01h - Seconds 
This register holds the count of seconds, in two binary coded decimal (BCD) digits. Values will be from 00 to 59.  
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
01h Seconds R/WP ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 Seconds 00 to 59 
Holds the count of seconds, coded in BCD format. 
When writing to the Seconds register the 100th Seconds register is cleared 
to 00 (similar to ESYN Bit function). When STOP bit is 1 the Seconds 
register value remains unchanged because 1 Hz clock is stopped (see 
TIME SYNCHRONIZATION). 
 
 
02h - Minutes 
This register holds the count of minutes, in two binary coded decimal (BCD) digits. Values will be from 00 to 59.  
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
02h Minutes R/WP ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 Minutes 00 to 59 Holds the count of minutes, coded in BCD format.
```

## Page 18

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 18/154 Rev. 1.3 
03h - Hours 
This register holds the count of hours, in two binary coded decimal (BCD) digits. Values will be from 00 to 23. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
03h Hours R/WP ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 Hours 00 to 23 Holds the count of hours, coded in BCD format. 
 
 
3.4. CALENDAR REGISTERS 
04h - Weekday 
This register holds the current day of the week. Each value represents one weekday that is assigned by the user. 
Values will range from 0 to 6. The weekday counter is simply a 3-bit counter which counts up to 6 and then resets to 
0. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
04h Weekday R/WP ○ ○ ○ ○ ○ 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:3 ○ 0 Read only. Always 0. 
2:0 Weekday 0 to 6 Holds the weekday counter value. 
 
Weekday Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
Weekday 1 - Default value 
0 0 0 0 0 
0 0 0 
Weekday 2 0 0 1 
Weekday 3 0 1 0 
Weekday 4 0 1 1 
Weekday 5 1 0 0 
Weekday 6 1 0 1 
Weekday 7 1 1 0 
 
 
05h - Date 
This register holds the current day of the month, in two binary coded decimal (BCD) digits. Values will range from 01 
to 31. Leap years are correctly handled from 2000 to 2099. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
05h Date R/WP ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 1 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 Date 01 to 31 Holds the current date of the month, coded in BCD format. 
 - Default value = 01 (01 is a valid date)
```

## Page 19

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 19/154 Rev. 1.3 
06h - Month 
This register holds the current month, in two  binary coded decimal (BCD) digits. Values will range from 01 to 12.  
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
06h Month R/WP ○ ○ ○ 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 1 
 
Bit Symbol Value Description 
7:5 ○ 0 Read only. Always 0. 
4:0 Month 01 to 12 Holds the current month, coded in BCD format. 
 
Months Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
January - Default value 
0 0 0 
0 0 0 0 1 
February 0 0 0 1 0 
March 0 0 0 1 1 
April 0 0 1 0 0 
May 0 0 1 0 1 
June 0 0 1 1 0 
July 0 0 1 1 1 
August 0 1 0 0 0 
September 0 1 0 0 1 
October 1 0 0 0 0 
November 1 0 0 0 1 
December 1 0 0 1 0 
 
 
07h - Year 
This register holds the current year, in two binary coded decimal (BCD) digits. Values will range from 00 to 99. Leap 
years are correctly handled from 2000 to 2099. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
07h Year R/WP 80 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 Year 00 to 99 Holds the current year, coded in BCD format. - Default value = 00
```

## Page 20

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 20/154 Rev. 1.3 
3.5. ALARM REGISTERS 
08h - Minutes Alarm 
This register holds the Minutes Alarm Enable bit AE_M and the alarm value for minutes, in two binary coded decimal 
(BCD) digits. Values will range from 00 to 59. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
08h Minutes Alarm R/WP AE_M 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 AE_M 
Minutes Alarm Enable bit. Enables alarm together with AE_H and AE_D 
(see USE OF THE ALARM INTERRUPT). 
0 Enabled. - Default value 
1 Disabled. 
6:0 Minutes Alarm 00 to 59 Holds the alarm value for minutes, coded in BCD format. 
 
 
09h - Hours Alarm 
This register holds the Hours Alarm Enable bit AE_H and the alarm value for hours, in two bin ary coded decimal 
(BCD) digits. Values will range from 00 to 23. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
09h Hours Alarm R/WP AE_H ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 AE_H 
Hours Alarm Enable bit. Enables alarm together with AE_M and AE_D 
(see USE OF THE ALARM INTERRUPT). 
0 Enabled. - Default value 
1 Disabled. 
6 ○ 0 Read only. Always 0. 
5:0 Hours Alarm 00 to 23 Holds the alarm value for hours, coded in BCD format. 
 
 
0Ah - Date Alarm 
This register holds the Date Alarm Enable bit AE_ D and the alarm value for the date, in two binary coded decimal 
(BCD) digits. Values will range from 01 to 31. Leap years are correctly handled from 2000 to 2099. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
0Ah Date Alarm R/WP AE_D ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 AE_D 
Date Alarm Enable bit. Enables alarm together with AE_M and AE_H 
(see USE OF THE ALARM INTERRUPT). 
0 Enabled. - Default value 
1 Disabled. 
6 ○ 0 Read only. Always 0. 
5:0 Date Alarm 01 to 31 
Holds the alarm value for the date, coded in BCD format. 
If the Date Alarm is to be used, the invalid value (00) must be replaced 
with a valid one (01 to 31). (1) 
(1) Note that an alarm is never generated after POR because of the default values AE_D = 0 = enabled, Date = 01 (valid) 
  and Date Alarm = 00h (not valid).
```

## Page 21

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 21/154 Rev. 1.3 
3.6. PERIODIC COUNTDOWN TIMER CONTROL REGISTERS 
0Bh - Timer Value 0 
This register is used to set the lower 8 bits of the 12 bit Timer Value (preset value) for the Periodic Countdown Timer. 
This value will be automatically reloaded into the Countdo wn Timer when it reaches zero . This allows for periodic 
timer interrupts (see calculation below). 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
0Bh Timer Value 0 R/WP 128 64 32 16 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 Timer Value 0 00h to 
FFh 
The Timer Value for the Periodic Countdown Timer in binary format (lower 
8 bit) (see USE OF THE PERIODIC COUNTDOWN TIMER). When read, 
only the preset value is returned and not the current value. 
When the Periodic Countdown Timer Interrupt function is not used, register 
0Bh can be used as RAM byte. 
 
 
0Ch - Timer Value 1 
This register is used to set the upper 4 bits of the 12 bit Timer Value (preset value) for the Periodic Countdown Timer. 
This value will be automatically reloaded into the Countdown Timer when it reaches zero. This allows for periodic 
timer interrupts (see calculation below). 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
0Ch Timer Value 1 R/WP ○ ○ ○ ○ 2048 1024 512 256 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:4 ○ 0 Read only. Always 0. 
3:0 Timer Value 1 0h to Fh 
The Timer Value for the Periodic Countdown Timer in binary format (upper 
4 bit) (see USE OF THE PERIODIC COUNTDOWN TIMER). When read, 
only the preset value is returned and not the current value. 
 
Countdown Period in seconds: 
 
Countdown Period = Timer Value
Timer Clock Frequency
```

## Page 22

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 22/154 Rev. 1.3 
3.7. STATUS REGISTER 
0Dh - Status 
This register is used to detect the occurrence of various interrupt events and reliability problems in internal data.  
Note that flags are read/clear only. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
0Dh Status R/WP THF TLF UF TF AF EVF PORF VLF 
Reset 0 0 0 0 0 X 1 0 
 
Bit Symbol Value Description 
7 THF 
Temperature High Flag (see TEMPERATURE HIGH INTERRUPT FUNCTION) 
0 No event detected. 
1 
If set to 0 beforehand, indicates the occurrence of a temperature that is 
above the stored Temperature High Threshold value THT. 
The value 1 is retained until a 0 is written by the user. (1) 
THF is also cleared to 0 when writing 1 to the THR bit. 
6 TLF 
Temperature Low Flag (see TEMPERATURE LOW INTERRUPT FUNCTION) 
0 No event detected. 
1 
If set to 0 beforehand, indicates the occurrence of a temperature that is 
below the stored Temperature Low Threshold value TLT. 
The value 1 is retained until a 0 is written by the user. (1) 
TLF is also cleared to 0 when writing 1 to the TLR bit. 
5 UF 
Periodic Time Update Flag 
(see PERIODIC TIME UPDATE INTERRUPT FUNCTION) 
0 No event detected. 
1 If set to 0 beforehand, indicates the occurrence of a Periodic Time Update 
Interrupt event. The value 1 is retained until a 0 is written by the user. 
4 TF 
Periodic Countdown Timer Flag 
(see PERIODIC COUNTDOWN TIMER INTERRUPT FUNCTION) 
0 No event detected. 
1 
If set to 0 beforehand, indicates the occurrence of a Periodic Countdown 
Timer Interrupt event. The value 1 is retained until a 0 is written by the 
user. 
3 AF 
Alarm Flag (see ALARM INTERRUPT FUNCTION) 
0 No event detected. 
1 
If set to 0 beforehand, indicates the occurrence of an Alarm Interrupt 
event. The value 1 is retained until a 0 is written by the user. 
Hint: The flag is set only on increment to a matched case (and not all the 
time it is equal). 
2 EVF 
External Event Flag (see EXTERNAL EVENT INTERRUPT FUNCTION) 
X 
The Reset value X depends on the voltage on the EVI pin at POR and has 
to be cleared by writing a 0 to the bit. Because EHL = 0 at POR, the low 
level is regarded as an External Event Interrupt. 
If X = 1, a LOW level was detected on EVI pin. 
If X = 0, no LOW level was detected on EVI pin. 
0 No event detected. 
1 If set to 0 beforehand, indicates the occurrence of an External Event. The 
value 1 is retained until a 0 is written by the user. 
1 PORF 
Power On Reset Flag (see POWER ON RESET INTERRUPT FUNCTION) 
0 No VDD startup from below VPOR (0.9 V) in VDD Power state detected. 
1 
If set to 0 beforehand, indicates that a VDD startup from below VPOR (0.9 V) 
in VDD Power state has occurred. The data in the device are no longer 
valid and all registers must be initialized. The value 1 is retained until a 0 is 
written by the user. At power up (POR) the value is set to 1, the user has 
to write 0 to the flag to use it. 
0 VLF 
Voltage Low Flag (see VOLTAGE LOW INTERRUPT FUNCTION) 
0 No voltage drop of the internal voltage below VLOW detected (VDD or 
VBACKUP). At power on (POR) the VLF flag is automatically cleared to 0. 
1 
If set to 0 beforehand, indicates a voltage drop below VLOW (typical 1.2 V). 
The sampling frequency is 1 Hz. The data in the device may no longer be 
valid and all registers should be reinitialized. The value 1 is retained until a 
0 is written by the user. 
If the internal voltage is below VLOW, the temperature compensation is 
stopped, CLKOUT is LOW and the I2C interface is disabled (VDD < 1.4 V). 
(1) Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 23

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 23/154 Rev. 1.3 
3.8. TEMPERATURE REGISTERS 
0Eh - Temperature LSBs 
This register hosts the 4 least significant bits (LSBs) of the Temperature value TEMP [11:0] in two’s complement 
format (fractional part). The register is also used to detect the occurrence of various interrupt events and reliability 
problems in internal data. 
Note that the flags (EEF, CLKF and BSF) are read/clear only. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
0Eh Temperature LSBs R/WP TEMP [3:0] EEF EEbusy CLKF BSF 
Reset 0h ? Xh 0 1 ? 0 0 0 
 
Bit Symbol Value Description 
7:4 TEMP [3:0] 0h to 
Fh 
Fractional part of the Temperature value TEMP [11:0] in two’s complement 
format. - (Read Only) 
Stores the last value of the measured internal temperature. Allows the 
higher measurement resolution of 1/16 = 0.0625°C. See table below (see 
also TEMPERATURE REFERENCE ADJUSTMENT). 
The internal temperature sensing itself is carried out automatically every 
second. One second after POR, the first temperature value (Xh) is 
available. 
Hint: The integer part TEMP [11:4] in register Temperature MSBs is 
automatically compared to the TLow and THigh Thresholds (see 
TEMPERATURE THRESHOLDS REGISTERS). 
3 EEF 
EEPROM Memory Write Access Failed Flag (see EEF FLAG) 
0 Previous write access was successful. 
1 
If set to 0 beforehand, indicates that the EEPROM write access has failed 
because VDD has dropped below VDD:EEF (1.3 V). The value 1 is retained 
until a 0 is written by the user. 
2 EEbusy 
EEPROM Memory Busy Status Bit - (Read Only) 
(see EEBUSY BIT) 
0 The transfer is finished. 
1 
Indicates that the EEPROM is currently handling a read or write request 
and will ignore any further commands until the current one is finished. 
At power up (POR) a refresh is automatically generated. The time of this 
first refreshment is tPREFR = ~66 ms. After the refreshment is finished; 
EEbusy is cleared to 0 automatically. 
1 CLKF 
Clock Output Interrupt Flag (see INTERRUPT CONTROLLED CLOCK OUTPUT) 
0 No event detected. 
1 
If set to 0 beforehand, indicates the occurrence of an interrupt driven clock 
output on CLKOUT pin. The value 1 is retained until a 0 is written by the 
user. 
0 BSF 
Backup Switch Flag (see AUTOMATIC BACKUP SWITCHOVER FUNCTION) 
0 
No backup switchover detected. At power up (POR) this flag is 
automatically cleared to 0. When the backup switchover function is 
disabled (BSM field = 00 or 11) BSF is always logic 0. 
1 
If set to 0 beforehand, indicates that a switchover from main power VDD to 
VBACKUP has occurred. The value 1 can be cleared by writing a 0 to the bit if 
RTC module is in VDD Power state.
```

## Page 24

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 24/154 Rev. 1.3 
0Fh - Temperature MSBs 
This register hosts the 8 most significant bits (MSBs)  of the Temperature value TEMP [11:0] in two’s complement 
format (integer part). 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
0Fh Temperature MSBs R TEMP [11:4] 
Reset 00h ? XXh 
 
Bit Symbol Value Description 
7:0 TEMP [11:4] 00h to 
FFh 
Integer part of the Temperature value TEMP [11:0] in two’s complement 
format.  - (Read Only) 
Stores the last value of the measured internal temperature with 1°C 
resolution in two’s complement format. See table below (see also 
TEMPERATURE REFERENCE ADJUSTMENT). 
The internal temperature sensing itself is carried out automatically every 
second. 
One second after POR, the first temperature value (XXh) is available. 
The TEMP [11:4] value is automatically compared to the TLow and THigh 
Thresholds (see TEMPERATURE THRESHOLDS REGISTERS). 
 
 
Temperature/Data relationship: 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
0Eh Temperature LSBs R/WP 2-1 2-2 2-3 2-4 EEF EEbusy CLKF BSF 
0Fh Temperature MSBs R Sign 26 25 24 23 22 21 20 
 
 
TEMP (12 bits) examples: 
TEMP [11:0] value Hexadecimal Decimal Signed decimal 
(two’s complement) Temperature in °C(*) 
0111'1111'1111 7FF 2047 2047 127.9375 
0110'1001'0000 690 1680 1680 105 
0101'0101'0000 550 1360 1360 85 
0100'1011'0000 4B0 1200 1200 75 
0011'0010'0000 320 800 800 50 
0001'1001'0000 190 400 400 25 
0000'0001'0000 010 16 16 1 
0000'0000'0100 004 4 4 0.25 
0000'0000'0001 001 1 1 0.0625 
0000'0000'0000 (default) 000 0 0 0 
1111'1111'1111 FFF 4095 -1 -0.0625 
1111'1111'1100 FFC 4092 -4 -0.25 
1111'1111'0000 FF0 4080 -16 -1 
1110'0111'0000 E70 3696 -400 -25 
1101'1000'0000 D80 3456 -640 -40 
1000'0000'0000 800 2048 -2048 -128 
(*) Temperature value in °C = Signed decimal / 16 = Signed decimal × 0.0625. 
 Note that there is no need to read the Temperature LSBs byte (TEMP [3:0]) if resolution below 1°C is not required. 
 Note that the thermometer must not be operated outside the temperature range from -40 to +105°C specified by the RV-3032-C7 module. 
 
Note: The Temperature LSBs and Temperature MSBs registers know no blocking/shadowing. To get a valid 12 -bit 
temperature value, the TEMP [11:0] value  should be read after the 1 Hz tick, or up to 1 ms before a 1 Hz tick (or 
TEMP [11:0] value can be read twice, and then compared).
```

## Page 25

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 25/154 Rev. 1.3 
3.9. CONTROL REGISTERS 
10h - Control 1 
This register is used to specify the source for the Periodic Time Update Interrupt func tion and to select or set 
operations for the Periodic Countdown Timer.  It also holds the control bit for automatic refresh of the Configuration 
Registers. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
10h Control 1 R/WP - - GP0 USEL TE EERD TD 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 - 0 Bit not implemented. Will return a 0 when read. 
5 GP0 0 or 1 Register bit for general purpose use. 
4 USEL 
Update Interrupt Select bit. Specifies either Second or Minute update for the Periodic 
Time Update Interrupt function. 
(see PERIODIC TIME UPDATE INTERRUPT FUNCTION). 
When STOP bit is set to 1 the interrupt function is stopped. When writing to the 
Seconds register or when ESYN bit is 1 in case of an External Event detection on EVI 
pin the length of the current update period is affected (see TIME 
SYNCHRONIZATION). 
0 Second update (Auto reset time tRTN2 = 500 ms). - Default value 
1 Minute update (Auto reset time tRTN2 = 15.6 ms). 
3 TE 
Periodic Countdown Timer Enable bit. This bit controls the start/stop setting for the 
Periodic Countdown Timer Interruption function 
(see PERIODIC COUNTDOWN TIMER INTERRUPT FUNCTION). 
0 Stops the Periodic Countdown Timer Interrupt function. - Default value 
1 Starts the Periodic Countdown Timer Interrupt function (a countdown starts 
from the preset value set in Timer Value registers). 
2 EERD 
EEPROM Memory Refresh Disable bit. When 1, disables the automatic refresh of the 
Configuration Registers from the EEPROM Memory 
(see AUTOMATIC REFRESH (ALL CONFIGURATION EEPROM ? RAM)). 
0 
Refresh is active. All data in the Configuration Registers are refreshed by 
the data stored in the EEPROM each 24 hours, at date increment (at the 
beginning of the last second before midnight). The time of this automatic 
refreshment is tAREFR = ~3.5 ms. Refresh is only active when RTC is not in 
VBACKUP Power state and VDD ≥ VLOW. - Default value 
1 Refresh is disabled. 
1:0 TD 00 to 11 
Timer Clock Frequency selection. Sets the countdown source clock for the 
Periodic Countdown Timer Interrupt function. With this setting the Auto 
reset time tRTN1 is also defined. See table below (see also PERIODIC 
COUNTDOWN TIMER INTERRUPT FUNCTION). 
When the clock source has been set to Second update (1 Hz) or Minute 
update (1/60 Hz), the timing of both, countdown and interrupts, is 
coordinated with the clock update timing but has a maximum jitter of 30.5 
µs. 
When STOP bit is set to 1 the interrupt function is stopped. When writing to 
the Seconds register or when ESYN bit is 1 in case of an External Event 
detection on EVI pin the length of the current countdown period is affected 
(see TIME SYNCHRONIZATION). 
 
TD value Timer Clock Frequency Countdown period tRTN1 STOP bit 
00 4096 Hz - Default value 244.14 Î¼s 122 Î¼s When STOP bit is set to 1 
the interrupt function is 
stopped (see also TIME 
SYNCHRONIZATION). 
01 64 Hz 15.625 ms 
7.813 ms 10 1 Hz 1 s 
11 1/60 Hz 60 s
```

## Page 26

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 26/154 Rev. 1.3 
11h - Control 2 
This register is used to enable the interrupt controlled clock output on CLKOUT pin and to control the interrupt event 
output for the INTÌ…Ì…Ì…Ì…Ì… pin and the stop/start status of clock and calendar operations. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
11h Control 2 R/WP - CLKIE UIE TIE AIE EIE GP1 STOP 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 - 0 Bit not implemented. Will return a 0 when read. 
6 CLKIE 
Interrupt Controlled Clock Output Enable bit. 
When enabled, it is possible to wake-up an external system by outputting a frequency. 
(see INTERRUPT CONTROLLED CLOCK OUTPUT) 
0 Disabled - Default value 
1 
When set to 1, the clock output on CLKOUT pin is automatically enabled 
when an interrupt occurs, based on the Clock Interrupt Mask (register 14h) 
and according to the clock setting defined in registers EEPROM Clkout 1 
and EEPROM Clkout 2 (C2h and C3h). This function is disabled in 
VBACKUP Power state (clock output when back in VDD Power state). 
5 UIE 
Periodic Time Update Interrupt Enable bit 
(see PERIODIC TIME UPDATE INTERRUPT FUNCTION) 
0 
No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin and UF flag is not set when a 
Periodic Time Update event occurs or the tRTN2 - signal on INTÌ…Ì…Ì…Ì…Ì… pin is 
cancelled. - Default value 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin and the UF flag is set when a 
Periodic Time Update event occurs. The low-level output signal is 
automatically cleared after tRTN2 = 500 ms (Second update) or 
tRTN2 = 15.6 ms (Minute update). (1) 
4 TIE 
Periodic Countdown Timer Interrupt Enable bit 
(see PERIODIC COUNTDOWN TIMER INTERRUPT FUNCTION) 
0 
No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when a Periodic Countdown 
Timer event occurs or the tRTN1 - signal on INTÌ…Ì…Ì…Ì…Ì… pin is cancelled. 
 - Default value 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when a Periodic Countdown 
Timer event occurs. The low-level output signal is automatically cleared 
after tRTN1 = 122 µs (TD = 00) or tRTN1 = 7.813 ms (TD = 01, 10, 11). (1) 
3 AIE 
Alarm Interrupt Enable bit (see ALARM INTERRUPT FUNCTION) 
0 No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when an Alarm event occurs or 
the signal is cancelled on INTÌ…Ì…Ì…Ì…Ì… pin. - Default value 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when an Alarm event occurs. 
The signal on INTÌ…Ì…Ì…Ì…Ì… pin is retained until the AF flag is cleared to 0 (no 
automatic cancellation). (1) 
2 EIE 
External Event Interrupt Enable bit 
(see EXTERNAL EVENT INTERRUPT FUNCTION and INTERRUPT SCHEME) 
0 No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when an External Event on EVI 
pin occurs or the signal is cancelled on INTÌ…Ì…Ì…Ì…Ì… pin. - Default value 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when an External Event on EVI 
pin occurs. The signal on INTÌ…Ì…Ì…Ì…Ì… pin is retained until the EVF flag is cleared to 
0 (no automatic cancellation). (1) 
1 GP1 0 or 1 Register bit for general purpose use. 
0 STOP 
Stop bit. This bit is used for a software-based time adjustment (synchronization) 
(see STOP BIT FUNCTION). 
0 Not stopped. - Default value 
1 
Stops and resets the clock prescaler frequencies from 4096 Hz to 1 Hz 
and the 100th Seconds register is reset to 00. 
A possible currently memorized 1 Hz update is also reset. 
The following functions are stopped: Clock and calendar (with alarm), 
CLKOUT, timer clock, update timer clock, EVI input filter, temperature 
measurement, temperature compensation and temperature comparison 
with THT and TLT values are stopped (see also TIME 
SYNCHRONIZATION). 
The External Event Interrupt function is still working but cannot provide 
useful data. 
(1) Interrupt Delay after CLKOUT On can be activated by setting bit INTDE.
```

## Page 27

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 27/154 Rev. 1.3 
12h - Control 3 
This register is used to enable temperature detections and to control the interrupt event output for the INTÌ…Ì…Ì…Ì…Ì… pin. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
12h Control 3 R/WP - - - BSIE THE TLE THIE TLIE 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:5 - 0 Bit not implemented. Will return a 0 when read. 
4 BSIE 
Backup Switchover Interrupt Enable bit 
(see AUTOMATIC BACKUP SWITCHOVER FUNCTION and 
AUTOMATIC BACKUP SWITCHOVER INTERRUPT FUNCTION) 
0 No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when an Automatic Backup 
Switchover occurs or the signal is cancelled on INTÌ…Ì…Ì…Ì…Ì… pin. - Default value 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when an Automatic Backup 
Switchover occurs. The signal on INTÌ…Ì…Ì…Ì…Ì… pin is retained until the BSF flag is 
cleared to 0 (no automatic cancellation). 
3 THE 
Temperature High Enable bit (see TEMPERATURE HIGH INTERRUPT FUNCTION) 
0 Temperature High detection is disabled. - Default value 
1 
Enables the Temperature High detection with programmable Temperature 
High Threshold, and the corresponding Time Stamp function. An event is 
generated every second, when TEMP [11:4] > THT. 
2 TLE 
Temperature Low Enable bit (see TEMPERATURE LOW INTERRUPT FUNCTION) 
0 Temperature Low detection is disabled. - Default value 
1 
Enables the Temperature Low detection with programmable Temperature 
Low Threshold, and the corresponding Time Stamp function. An event is 
generated every second, when TEMP [11:4] < TLT. 
1 THIE 
Temperature High Interrupt Enable bit 
(see TEMPERATURE HIGH INTERRUPT FUNCTION and INTERRUPT SCHEME) 
0 No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when Temperature High is 
detected or the signal is cancelled on INTÌ…Ì…Ì…Ì…Ì… pin. - Default value 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when Temperature High is 
detected. The signal on INTÌ…Ì…Ì…Ì…Ì… pin is retained until the THF flag is cleared to 0 
(no automatic cancellation). (1) (2) 
0 TLIE 
Temperature Low Interrupt Enable bit 
(see TEMPERATURE LOW INTERRUPT FUNCTION and INTERRUPT SCHEME) 
0 No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when Temperature Low is 
detected or the signal is cancelled on INTÌ…Ì…Ì…Ì…Ì… pin. - Default value 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when Temperature Low is 
detected. The signal on INTÌ…Ì…Ì…Ì…Ì… pin is retained until the TLF flag is cleared to 0 
(no automatic cancellation). (1) (2) 
(1) Interrupt Delay after CLKOUT On can be activated by setting bit INTDE. 
(2) Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 28

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 28/154 Rev. 1.3 
3.10. TIME STAMP CONTROL REGISTER 
13h - Time Stamp Control 
This register holds the control bits for the Time Stamp data. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
13h 
Time Stamp 
Control R/WP ○ ○ EVR THR TLR EVOW THOW TLOW 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5 EVR 
Time Stamp EVI Reset bit (see TIME STAMP EVI FUNCTION) 
0 Disabled. - Default value 
1 
Writing 1 to the EVR bit resets all eight Time Stamp EVI registers (TS EVI 
Count to TS EVI Year) to 00h. 
EVR may remain set. No further reset occurs. 
4 THR 
Time Stamp THigh Reset bit (see TIME STAMP THIGH FUNCTION) 
0 Disabled. - Default value 
1 
Writing 1 to the THR bit resets all seven Time Stamp THigh registers (TS 
THigh Count to TS THigh Year) to 00h and the THF flag is also cleared to 
0. The THR bit always returns 0 when read. 
3 TLR 
Time Stamp TLow Reset bit (see TIME STAMP TLOW FUNCTION) 
0 Disabled. - Default value 
1 
Writing 1 to the TLR bit resets all seven Time Stamp TLow registers (TS 
TLow Count to TS TLow Year) to 00h and the TLF flag is also cleared to 0. 
The TLR bit always returns 0 when read. 
2 EVOW 
Time Stamp EVI Overwrite bit. Controls the overwrite function of the TS EVI registers 
(TS EVI 100th Seconds to TS EVI Year). The TS EVI Count register always counts 
events, regardless of the settings of the override bit EVOW. 
(see TIME STAMP EVI FUNCTION) 
0 
The time stamp of the first occurred event is recorded and remains in TS 
EVI registers.- Default value 
To initialize or reinitialize the first event detection function, 1 has to be 
written to the EVR bit to clear all TS EVI registers (POR has same effect, 
when EVI pin = HIGH). 
Caution: For the Time Stamp EVI function, only the TS EVI Count register 
is responsible for detecting first or last event, and therefore, always after 
an overflow of the TS EVI Count register from 255 to 0, a new First Event 
is allowed by the function (see also TIME STAMP EVI SCHEME). 
1 The time stamp of the last occurred event is recorded and TS EVI registers 
are overwritten. 
1 THOW 
Time Stamp THigh Overwrite bit. Controls the overwrite function of the TS THigh 
registers (TS THigh Seconds to TS THigh Year). The TS THigh Count register always 
counts events, regardless of the settings of the override bit THOW. 
(see TIME STAMP THIGH FUNCTION) 
0 
The time stamp of the first occurred event is recorded and remains in TS 
THigh registers. - Default value 
To initialize or reinitialize the first event detection function, 1 has to be 
written to the THR bit to clear all TS THigh registers (POR has same 
effect). 
1 The time stamp of the last occurred event is recorded and TS THigh 
registers are overwritten. 
0 TLOW 
Time Stamp TLow Overwrite bit. Controls the overwrite function of the TS TLow 
registers (TS TLow Seconds to TS TLow Year). The TS TLow Count register always 
counts events, regardless of the settings of the override bit TLOW. 
(see TIME STAMP TLOW FUNCTION) 
0 
The time stamp of the first occurred event is recorded and remains in TS 
TLow registers. - Default value 
To initialize or reinitialize the first event detection function, 1 has to be 
written to the TLR bit clear all TS TLow registers (POR has same effect). 
1 The time stamp of the last occurred event is recorded and TS TLow 
registers are overwritten.
```

## Page 29

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 29/154 Rev. 1.3 
3.11. CLOCK INTERRUPT MASK REGISTER 
14h - Clock Interrupt Mask 
This register is used select a  CLKOUT off Delay Value after I2C STOP and  to enable the Interrupt Delay after 
CLKOUT On. It is also used to select a predefined interrupt for Interrupt Controlled Clock Output . Setting a bit to 1 
selects the corresponding interrupt. Multiple interrupts can be selected. After power on, no interrupt is selected (see  
INTERRUPT SCHEME and CLOCK OUTPUT SCHEME). 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
14h 
Clock Interrupt 
Mask R/WP CLKD INTDE CEIE CAIE CTIE CUIE CTHIE CTLIE 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 CLKD 
CLKOUT (switch) off Delay Value after I2C STOP Selection bit. 
Applicable only when CLKDE bit in the EVI Control register is set to 1. 
(see CLKOUT OFF DELAY AFTER I2C STOP) 
0 Typical delay time tI2C:CLK = 1.4 ms. - Default value 
1 Typical delay time tI2C:CLK = 75 ms. 
6 INTDE 
Interrupt Delay after CLKOUT On Enable bit. 
Applicable only when NCLKE bit in the EEPROM PMU register is set to 1 (CLKOUT 
not directly enabled) and for interrupts enabled by CEIE, CAIE, CTIE, CUIE, CTHIE or 
CTLIE (see INTERRUPT DELAY AFTER CLKOUT ON) 
0 No delay. - Default value 
1 Enables the delay time tCLK:INT of 1/256 seconds to 3/512 seconds ≈ 3.9 ms 
to 5.9 ms. 
5 CEIE 
Clock output when EVI Interrupt Enable bit. 
0 Disabled - Default value 
1 Enabled. Internal signal EI is selected. (1) 
4 CAIE 
Clock output when Alarm Interrupt Enable bit. 
0 Disabled - Default value 
1 Enabled. Internal signal AI is selected. (1) 
3 CTIE 
Clock output when Periodic Countdown Timer Interrupt Enable bit. 
0 Disabled - Default value 
1 Enabled: Internal signal TI is selected. (1) 
If TD = 00 (4096 Hz) is selected, no interrupt delay is added. 
2 CUIE 
Clock output when Periodic Time Update Interrupt Enable bit. 
0 Disabled - Default value 
1 Enabled. Internal signal UI is selected. (1) 
1 CTHIE 
Clock output when THigh Interrupt Enable bit. 
0 Disabled - Default value 
1 Enabled. Internal signal THI is selected. (1) 
0 CTLIE 
Clock output when TLow Interrupt Enable bit 
0 Disabled - Default value 
1 Enabled. Internal signal TLI is selected. (1) 
(1) Interrupt Delay after CLKOUT On can be activated by setting bit INTDE.
```

## Page 30

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 30/154 Rev. 1.3 
3.12. EVI CONTROL REGISTER 
15h - EVI Control 
This register controls the event detection on the EVI pin. Depending of the EHL bit, high or low level (or rising or 
falling edge) can be detected. Moreover a digital glitch filtering can be applied to the EVI signal by selecting a 
sampling period tSP in the ET field. Furthermore this register holds the enable bit for the CLKOUT off Delay after I 2C 
STOP and the External Event Synchronization bit. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
15h EVI Control R/WP CLKDE EHL ET ○ ○ ○ ESYN 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 CLKDE 
CLKOUT (switch) off Delay after I2C STOP Enable bit 
0 Disabled - Default value 
1 Enabled. The delay time tI2C:CLK can be selected with CLKD bit in Clock 
Interrupt Mask register. 
6 EHL 
Event High/Low Level (Rising/Falling Edge) selection for detection 
(see EXTERNAL EVENT INTERRUPT FUNCTION) 
0 The falling edge (ET = 00) or low level (ET ≠ 00) is regarded as the 
External Event on pin EVI. - Default value 
1 The rising edge (ET = 00) or high level (ET ≠ 00) is regarded as the 
External Event on pin EVI. 
5:4 ET 
Event Filtering Time set. 
Applies a digital filtering to the EVI pin by sampling the EVI signal. 
(see EXTERNAL EVENT INTERRUPT FUNCTION). 
00 No filtering. Edge detection. - Default value 
01 Sampling period tSP = 3.9 ms (256 Hz). Edge & Level detection. 
10 Sampling period tSP = 15.6 ms (64 Hz). Edge & Level detection. 
11 Sampling period tSP = 125 ms (8 Hz). Edge & Level detection. 
3:1 ○ 0 Read only. Always 0. 
0 ESYN 
External Event (EVI) Synchronization bit. 
This bit is used for a hardware-based time adjustment (see ESYN BIT FUNCTION). 
0 Disabled - Default value 
1 
In case of an External Event detection at the EVI pin the clock prescaler 
frequencies from 4096 Hz to 1 Hz are reset and the 100th Seconds register 
is reset to 00. A possible currently memorized 1 Hz update is also reset. 
When an External Event occurs, the Time Stamp EVI is always created 
first and then the 100th Seconds register is cleared to 00. 
After the event detection, the ESYN bit is reset to 0 automatically. 
If 1, the synchronization function can be canceled by resetting the ESYN 
bit to 0 before an event occurs.
```

## Page 31

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 31/154 Rev. 1.3 
3.13. TEMPERATURE THRESHOLDS REGISTERS 
16h - TLow Threshold 
In this register, the user can define the Temperature Low Threshold value TLT which is compared with the TEMP 
[11:4] value in the Temperature MSBs register . TLT is stored in the same two’s complement format  as the TEMP 
[11:4] (see TEMPERATURE REGISTERS). 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
16h TLow Threshold R/WP TLT 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 TLT -128 to 
127 
Temperature Low Threshold value with 1°C resolution in two’s complement 
format like TEMP [11:4]. The integer part TEMP [11:4] from the internal 
temperature is automatically compared to this value. An event is generated 
when TEMP [11:4] < TLT (see TEMPERATURE REGISTERS, 
TEMPERATURE LOW INTERRUPT FUNCTION and TIME STAMP TLOW 
FUNCTION). 
 
 
17h - THigh Threshold 
In this register, the user can define  the Temperature High Threshold value THT which is compared with the TEMP 
[11:4] value in the Temperature MSBs register. THT is stored in the same two’s complement format as the TEMP 
[11:4] (see TEMPERATURE REGISTERS). 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
17h THigh Threshold R/WP THT 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 THT -128 to 
127 
Temperature High Threshold value with 1°C resolution in two’s 
complement format like TEMP [11:4]. The integer part TEMP [11:4] from 
the internal temperature is automatically compared to this value. An event 
is generated when TEMP [11:4] > THT (see TEMPERATURE 
REGISTERS, TEMPERATURE HIGH INTERRUPT FUNCTION and TIME 
STAMP THIGH FUNCTION).
```

## Page 32

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 32/154 Rev. 1.3 
3.14. TIME STAMP TLOW REGISTERS 
Seven Time Stamp TLow registers (TS TLow Count and TS TLow Seconds to TS TLow Year), (see TIME STAMP 
TLOW FUNCTION). 
 
18h - TS TLow Count 
This register contains the number of occurrences of Temperature Low events (TEMP [11:4] < TLT) in standard binary 
format. The values range from 0 to 255. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
18h TS TLow Count R 128 64 32 16 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 TS TLow Count 0 to 255 
Number of occurrences of Temperature Low events, coded in binary. In 
case of an overflow the counter starts again with 00h. 
When bit TLE = 0, the counter stops counting events. 
When bit TLE = 1, the counter is increased when event occurs. 
The TS TLow Count register always counts events, regardless of the 
settings of the override bit TLOW. 
The TS TLow Count register is reset to 00h when 1 is written to the Time 
Stamp TLow Reset bit TLR (see TIME STAMP TLOW FUNCTION). 
 
 
19h - TS TLow Seconds 
This register holds a recorded Temperature Low Time Stamp of the Seconds register, in two binary coded decimal 
(BCD) digits. The values are from 00 to 59. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
19h TS TLow Seconds R ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 TS TLow Seconds 00 to 59 
Holds a recorded Temperature Low Time Stamp of the Seconds register, 
coded in BCD format. When enabled (bit TLE = 1), depending on the 
setting of the TLOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS TLow Seconds register is reset to 00h when 1 is written to the 
Time Stamp TLow Reset bit TLR (see TIME STAMP TLOW FUNCTION). 
 
 
1Ah - TS TLow Minutes 
This register holds a recorded Temperature Low Time Stamp of the Minutes register, in two binary coded decimal 
(BCD) digits. The values are from 00 to 59. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
1Ah TS TLow Minutes R ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 TS TLow Minutes 00 to 59 
Holds a recorded Temperature Low Time Stamp of the Minutes register, 
coded in BCD format. When enabled (bit TLE = 1), depending on the 
setting of the TLOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS TLow Minutes register is reset to 00h when 1 is written to the Time 
Stamp TLow Reset bit TLR (see TIME STAMP TLOW FUNCTION).
```

## Page 33

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 33/154 Rev. 1.3 
1Bh - TS TLow Hours 
This register holds a recorded Temperature Low Time Stamp of the Hours register, in two binary  coded decimal 
(BCD) digits. Values will range from 00 to 23. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
1Bh TS TLow Hours R ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 TS TLow Hours 00 to 23 
Holds a recorded Temperature Low Time Stamp of the Hours register, 
coded in BCD format. When enabled (bit TLE = 1), depending on the 
setting of the TLOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS TLow Hours register is reset to 00h when 1 is written to the Time 
Stamp TLow Reset bit TLR (see TIME STAMP TLOW FUNCTION). 
 
 
1Ch - TS TLow Date 
This register holds a recorded Temperature Low Time Stamp of the Date register, in two binary coded decimal (BCD) 
digits. The values will range from 01 to 31. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
1Ch TS TLow Date R ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 TS TLow Date 01 to 31 
Holds a recorded Temperature Low Time Stamp of the Date register, 
coded in BCD format. When enabled (bit TLE = 1), depending on the 
setting of the TLOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS TLow Date register is reset to 00h when 1 is written to the Time 
Stamp TLow Reset bit TLR (see TIME STAMP TLOW FUNCTION). 
The value 00 after POR or after the reset with the TLR bit, is replaced by a 
valid value (01 to 31) when a Temperature Low Time Stamp is recorded. 
 
 
1Dh - TS TLow Month 
This register holds a recorded Temperature Low Time Stamp of the Month register, in two binary coded decimal 
(BCD) digits. The values will range from 01 to 12. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
1Dh TS TLow Month R ○ ○ ○ 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:5 ○ 0 Read only. Always 0. 
4:0 TS TLow Month 01 to 12 
Holds a recorded Temperature Low Time Stamp of the Month register, 
coded in BCD format. When enabled (bit TLE = 1), depending on the 
setting of the TLOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS TLow Month register is reset to 00h when 1 is written to the Time 
Stamp TLow Reset bit TLR (see TIME STAMP TLOW FUNCTION). 
The value 00 after POR or after the reset with the TLR bit, is replaced by a 
valid value (01 to 12) when a Temperature Low Time Stamp is recorded.
```

## Page 34

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 34/154 Rev. 1.3 
1Eh - TS TLow Year 
This register holds a recorded Temperature Low Time Stamp of the Year register, in two binary coded decimal (BCD) 
digits. Values will range from 00 to 99. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
1Eh TS TLow Year R 80 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 TS TLow Year 00 to 99 
Holds a recorded Temperature Low Time Stamp of the Year register, 
coded in BCD format. When enabled (bit TLE = 1), depending on the 
setting of the TLOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS TLow Year register is reset to 00h when 1 is written to the Time 
Stamp TLow Reset bit TLR (see TIME STAMP TLOW FUNCTION).
```

## Page 35

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 35/154 Rev. 1.3 
3.15. TIME STAMP THIGH REGISTERS 
Seven Time Stamp THigh registers (TS THigh Count and TS THigh Seconds to TS THigh Year), (see TIME STAMP 
THIGH FUNCTION). 
 
1Fh - TS THigh Count 
This register contains the number of occurrences of Temperature High events ( TEMP [11:4] > THT ) in standard 
binary format. The values range from 0 to 255. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
1Fh TS THigh Count R 128 64 32 16 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 TS THigh Count 0 to 255 
Number of occurrences of Temperature High events, coded in binary. In 
case of an overflow the counter starts again with 00h. 
When bit THE = 0, the counter stops counting events. 
When bit THE = 1, the counter is increased when event occurs. 
The TS THigh Count register always counts events, regardless of the 
settings of the override bit THOW. 
The TS THigh Count register is reset to 00h when 1 is written to the Time 
Stamp THigh Reset bit THR (see TIME STAMP THIGH FUNCTION). 
 
 
20h - TS THigh Seconds 
This register holds a recorded Temperature High Time Stamp of the Seconds register, in two binary coded decimal 
(BCD) digits. The values are from 00 to 59. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
20h TS THigh Seconds R ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 TS THigh Seconds 00 to 59 
Holds a recorded Temperature High Time Stamp of the Seconds register, 
coded in BCD format. When enabled (bit THE = 1), depending on the 
setting of the THOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS THigh Seconds register is reset to 00h when 1 is written to the 
Time Stamp THigh Reset bit THR (see TIME STAMP THIGH FUNCTION). 
 
 
21h - TS THigh Minutes 
This register holds a recorded Temperature High Time Stamp of the Minutes register, in two binary coded decimal 
(BCD) digits. The values are from 00 to 59. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
21h TS THigh Minutes R ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 TS THigh Minutes 00 to 59 
Holds a recorded Temperature High Time Stamp of the Minutes register, 
coded in BCD format. When enabled (bit THE = 1), depending on the 
setting of the THOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS THigh Minutes register is reset to 00h when 1 is written to the Time 
Stamp THigh Reset bit THR (see TIME STAMP THIGH FUNCTION).
```

## Page 36

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 36/154 Rev. 1.3 
22h - TS THigh Hours 
This register holds a recorded Temperature High Time Stamp of the Hours register, in two binary  coded decimal 
(BCD) digits. Values will range from 00 to 23. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
22h TS THigh Hours R ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 TS THigh Hours 00 to 23 
Holds a recorded Temperature High Time Stamp of the Hours register, 
coded in BCD format. When enabled (bit THE = 1), depending on the 
setting of the THOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS THigh Hours register is reset to 00h when 1 is written to the Time 
Stamp THigh Reset bit THR (see TIME STAMP THIGH FUNCTION). 
 
 
23h - TS THigh Date 
This register holds a recorded Temperature High Time Stamp of the Date register, in two binary coded decimal (BCD) 
digits. The values will range from 01 to 31. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
23h TS THigh Date R ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 TS THigh Date 01 to 31 
Holds a recorded Temperature High Time Stamp of the Date register, 
coded in BCD format. When enabled (bit THE = 1), depending on the 
setting of the THOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS THigh Date register is reset to 00h when 1 is written to the Time 
Stamp THigh Reset bit THR (see TIME STAMP THIGH FUNCTION). 
The value 00 after POR or after the reset with the THR bit, is replaced by a 
valid value (01 to 31) when a Temperature High Time Stamp is recorded. 
 
 
24h - TS THigh Month 
This register holds a recorded Temperature High Time Stamp of the Month register, in two binary coded decimal 
(BCD) digits. The values will range from 01 to 12. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
24h TS THigh Month R ○ ○ ○ 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:5 ○ 0 Read only. Always 0. 
4:0 TS THigh Month 01 to 12 
Holds a recorded Temperature High Time Stamp of the Month register, 
coded in BCD format. When enabled (bit THE = 1), depending on the 
setting of the THOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS THigh Month register is reset to 00h when 1 is written to the Time 
Stamp THigh Reset bit THR (see TIME STAMP THIGH FUNCTION). 
The value 00 after POR or after the reset with the THR bit, is replaced by a 
valid value (01 to 12) when a Temperature Low Time Stamp is recorded.
```

## Page 37

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 37/154 Rev. 1.3 
25h - TS THigh Year 
This register holds a recorded Temperature High Time Stamp of the Year register, in two binary coded decimal (BCD) 
digits. Values will range from 00 to 99. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
25h TS THigh Year R 80 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 TS THigh Year 00 to 99 
Holds a recorded Temperature High Time Stamp of the Year register, 
coded in BCD format. When enabled (bit THE = 1), depending on the 
setting of the THOW bit, it contains the time stamp of the first or last 
occurred event. 
The TS THigh Year register is reset to 00h when 1 is written to the Time 
Stamp THigh Reset bit THR (see TIME STAMP THIGH FUNCTION).
```

## Page 38

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 38/154 Rev. 1.3 
3.16. TIME STAMP EVI REGISTERS 
Eight Time Stamp EVI registers (TS EVI Count and TS EVI 100 th Seconds to TS EVI Year), (see TIME STAMP EVI 
FUNCTION). 
 
26h - TS EVI Count 
This register contains the number of occurrences of External Events on EVI pin in standard binary format. The values 
range from 0 to 255. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
26h TS EVI Count R 128 64 32 16 8 4 2 1 
Reset 0 0 0 0 0 0 0 X 
 
Bit Symbol Value Description 
7:0 TS EVI Count 0 to 255 
Number of occurrences of External Events on EVI pin, coded in binary. In 
case of an overflow the counter starts again with 00h. 
The TS EVI Count register always counts events, regardless of the 
settings of the override bit EVOW. 
The TS EVI Count register is reset to 00h when 1 is written to the Time 
Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION). 
 
The Reset value X depends on the voltage on the EVI pin at POR. 
Because EHL = 0 at POR, the low level is regarded as an External Event 
Interrupt. 
If X = 1, a LOW level was detected on EVI pin. 
If X = 0, no LOW level was detected on EVI pin. 
 
 
27h - TS EVI 100th Seconds 
This register holds a recorded External Event Time Stamp of the 100th Seconds register, in two binary coded decimal 
(BCD) digits. The values are from 00 to 99. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
27h 
TS EVI 100th 
Seconds R 80 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 TS EVI 100th Seconds 00 to 99 
Holds a recorded External Event Time Stamp of the 100th Seconds 
register, coded in BCD format. 
Depending on the setting of the EVOW bit, it contains the time stamp of 
the first or last occurred event. 
The TS EVI 100th Seconds register is reset to 00h when 1 is written to the 
Time Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION).
```

## Page 39

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 39/154 Rev. 1.3 
28h - TS EVI Seconds 
This register holds a recorded External Event Time Stamp of the Seconds register, in two binary coded decimal 
(BCD) digits. The values are from 00 to 59. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
28h TS EVI Seconds R ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 TS EVI Seconds 00 to 59 
Holds a recorded External Event Time Stamp of the Seconds register, 
coded in BCD format. 
Depending on the setting of the EVOW bit, it contains the time stamp of 
the first or last occurred event. 
The TS EVI Seconds register is reset to 00h when 1 is written to the Time 
Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION). 
 
 
29h - TS EVI Minutes 
This register holds a recorded External Event Time Stamp of the Minutes register, in two binary coded decimal (BCD) 
digits. The values are from 00 to 59. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
29h TS EVI Minutes R ○ 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 ○ 0 Read only. Always 0. 
6:0 TS EVI Minutes 00 to 59 
Holds a recorded External Event Time Stamp of the Minutes register, 
coded in BCD format. 
Depending on the setting of the EVOW bit, it contains the time stamp of 
the first or last occurred event. 
The TS EVI Minutes register is reset to 00h when 1 is written to the Time 
Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION). 
 
 
2Ah - TS EVI Hours 
This register holds a recorded External Event Time Stamp of the Hours register, in two binary coded decimal (BCD) 
digits. Values will range from 00 to 23. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
2Ah TS EVI Hours R ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 TS EVI Hours 0 to 23 
Holds a recorded External Event Time Stamp of the Hours register, coded 
in BCD format. 
Depending on the setting of the EVOW bit, it contains the time stamp of 
the first or last occurred event. 
The TS EVI Hours register is reset to 00h when 1 is written to the Time 
Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION).
```

## Page 40

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 40/154 Rev. 1.3 
2Bh - TS EVI Date 
This register holds a recorded External Event Time Stamp of the Date register, in two binary coded decimal (BCD) 
digits. The values will range from 01 to 31. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
2Bh TS EVI Date R ○ ○ 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 X 
 
Bit Symbol Value Description 
7:6 ○ 0 Read only. Always 0. 
5:0 TS EVI Date 01 to 31 
Holds a recorded External Event Time Stamp of the Date register, coded in 
BCD format. 
Depending on the setting of the EVOW bit, it contains the time stamp of 
the first or last occurred event. 
The TS EVI Date register is reset to 00h when 1 is written to the Time 
Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION). 
 
The Reset value X depends on the voltage on the EVI pin at POR. 
Because EHL = 0 at POR, the low level is regarded as an External Event 
Interrupt and an External Event Time Stamp is recorded. 
If X = 1, a LOW level was detected on EVI pin. 
If X = 0, no LOW level was detected on EVI pin. 
 
The value 00 after POR (if EVI-Pin = HIGH), or after the reset with the EVR 
bit, is replaced by a valid value (01 to 31) when an External Event Time 
Stamp is recorded. 
 
 
2Ch - TS EVI Month 
This register holds a recorded External Event Time Stamp of the Month register, in two binary coded decimal (BCD) 
digits. The values will range from 01 to 12. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
2Ch TS EVI Month R ○ ○ ○ 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 X 
 
Bit Symbol Value Description 
7:5 ○ 0 Read only. Always 0. 
4:0 TS EVI Month 01 to 12 
Holds a recorded External Event Time Stamp of the Month register, coded 
in BCD format. 
Depending on the setting of the EVOW bit, it contains the time stamp of 
the first or last occurred event. 
The TS EVI Month register is reset to 00h when 1 is written to the Time 
Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION). 
 
The Reset value X depends on the voltage on the EVI pin at POR. 
Because EHL = 0 at POR, the low level is regarded as an External Event 
Interrupt and an External Event Time Stamp is recorded. 
If X = 1, a LOW level was detected on EVI pin. 
If X = 0, no LOW level was detected on EVI pin. 
 
The value 00 after POR (if EVI-Pin = HIGH), or after the reset with the EVR 
bit, is replaced by a valid value (01 to 12) when an External Event Time 
Stamp is recorded.
```

## Page 41

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 41/154 Rev. 1.3 
2Dh - TS EVI Year 
This register holds a recorded External Event Time Stamp of the Year register, in two binary coded decimal (BCD) 
digits. Values will range from 00 to 99. 
Read only. Writing to this register has no effect. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
2Dh TS EVI Year R 80 40 20 10 8 4 2 1 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 TS EVI Year 00 to 99 
Holds a recorded External Event Time Stamp of the Year register, coded in 
BCD format. 
Depending on the setting of the EVOW bit, it contains the time stamp of 
the first or last occurred event. 
The TS EVI Year register is reset to 00h when 1 is written to the Time 
Stamp EVI Reset bit EVR (see TIME STAMP EVI FUNCTION).
```

## Page 42

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 42/154 Rev. 1.3 
3.17. PASSWORD REGISTERS 
After a Power up and the first refreshment time tPREFR = ~66 ms, the Password PW registers are reset to 00h. 
When the password function is enabled (EEPWE = 255), the correct 32-Bit Password must be written to the Password 
PW registers to write to the registers with the WP convention (time, control, user RAM, configuration EEPROM and 
user EEPROM registers).  The 32-Bit Password PW is compared with  the 32 bits in the RAM mirror of the EEPW 
registers (see EEPROM PASSWORD REGISTERS). 
 
39h - Password 0 
Bit 0 to 7 from 32-bit Password. 
Write only. Returns 0 when read. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
39h Password 0 W PW [7:0] 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 PW [7:0] 00h to 
FFh Bit 0 to 7 from 32-bit Password 
 
3Ah - Password 1 
Bit 8 to 15 from 32-bit Password. 
Write only. Returns 0 when read. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
3Ah Password 1 W PW [15:8] 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 PW [15:8] 00h to 
FFh Bit 8 to 15 from 32-bit Password 
 
3Bh - Password 2 
Bit 16 to 23 from 32-bit Password. 
Write only. Returns 0 when read. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
3Bh Password 2 W PW [23:16] 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 PW [23:16] 00h to 
FFh Bit 16 to 23 from 32-bit Password 
 
3Ch - Password 3 
Bit 24 to 31 from 32-bit Password. 
Write only. Returns 0 when read. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
3Ch Password 3 W PW [31:24] 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 PW [31:24] 00h to 
FFh Bit 24 to 31 from 32-bit Password
```

## Page 43

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 43/154 Rev. 1.3 
3.18. EEPROM MEMORY CONTROL REGISTERS 
See also EEPROM READ/WRITE. 
 
3Dh - EE Address 
This register holds the Address used for read or write from/to a single EEPROM Memory byte. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
3Dh EE Address R/WP EEADDR 
Reset 1 1 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EEADDR 00h to 
FFh 
Address for direct read or write one EEPROM Memory byte. 
 - Default value = C0h 
The default address C0h points to the first Configuration EEPROM 
Register (EEPROM PMU) 
 
 
3Eh - EE Data 
This register holds the Data that are read from, or that are written to a single EEPROM Memory byte. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
3Eh EE Data R/WP EEDATA 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EEDATA 00h to 
FFh 
Data from direct read or for direct write to one EEPROM Memory byte. 
 - Default value = 00h
```

## Page 44

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 44/154 Rev. 1.3 
3Fh - EE Command 
This register must be written with specific values, in order to Update or Refresh all (readable/writeable) Configuration 
EEPROM registers or to read or write from/to a single EEPROM Memory byte. 
Before using this commands, the automatic refresh function has to be disabled (EERD = 1) and the busy status bit 
EEbusy has to indicate that the last transfer has been finished (EEbusy = 0). The EEF flag can be used for EEPROM 
write access failure detection. Other values, unless 11h, 12h, 21h or 22h, should not be entered. 
Write only. Returns 0 when read. Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
3Fh EE Command WP EECMD 
Reset 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EECMD 
Commands for EEPROM Memory (see EEPROM READ/WRITE) 
Other values, unless 11h, 12h, 21h or 22h, should not be entered. 
11h 
UPDATE (ALL CONFIGURATION RAM ? EEPROM). 
When writing a value of 11h, data from all Configuration RAM mirror bytes 
(address C0h to CAh) are written (stored) into the corresponding 
Configuration EEPROM bytes. See also USE OF THE CONFIGURATION 
REGISTERS. 
12h 
REFRESH (ALL CONFIGURATION EEPROM ? RAM). 
When writing a value of 12h, data from all Configuration EEPROM bytes 
are read and copied into the corresponding Configuration RAM mirror 
bytes (address C0h to CAh). Functions become active as soon as the 
RAM bytes are written. 
21h 
WRITE TO ONE EEPROM BYTE (EEDATA (RAM) ? EEPROM). 
When writing a value of 21h, data from the EEDATA (RAM) byte are 
written (stored) into the EEPROM byte with the address specified in the 
EEADDR byte. For Configuration EEPROM bytes (address C0h to CAh) 
and User EEPROM bytes (address CBh to EAh). 
22h 
READ ONE EEPROM BYTE (EEPROM ? EEDATA (RAM)). 
When writing a value of 22h, data from the EEPROM byte with the address 
specified in EEADDR byte are read and copied into the EEDATA (RAM) 
byte. For Configuration EEPROM bytes (address C0h to CAh) and User 
EEPROM bytes (address CBh to EAh). 
 
 
3.19. RAM REGISTERS 
40h to 4Fh - User RAM 
16 Bytes of User RAM for general purpose storage are provided. For example, they can be used to store system 
status bytes. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
40h to 4Fh User RAM R/WP 16 Bytes of User RAM. - Default values are 00h
```

## Page 45

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 45/154 Rev. 1.3 
3.20. CONFIGURATION EEPROM WITH RAM MIRROR REGISTERS 
All Configuration EEPROM registers at addresses C0h to CAh are memorized in the EEPROM and mirrored in the 
RAM. Functions become active  as soon as the RAM mirror bytes are written. See also USE OF THE 
CONFIGURATION REGISTERS. 
 
 
3.20.1. EEPROM PMU REGISTER 
C0h - EEPROM Power Management Unit (PMU) 
This register is used to control the switchover function, the trickle charger with charge pump and it holds the NCLKE 
bit (see PROGRAMMABLE CLOCK OUTPUT). 
After a Power up and the first refreshment time tPREFR = ~66 ms, the EEPROM PMU register value is copied from the 
EEPROM to the corresponding RAM mirror. The default value preset on delivery is 00h. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C0h EEPROM PMU R/WP - NCLKE BSM TCR TCM 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 - 0 Bit not implemented. Will return a 0 when read. 
6 NCLKE 
Not CLKOUT Enable bit (see PROGRAMMABLE CLOCK OUTPUT) 
(synchronized enable/disable) 
0 CLKOUT is directly enabled. - Default value on delivery 
1 CLKOUT pin is LOW, if not enabled by the interrupt driven clock output 
(CLKF = 0) (see also INTERRUPT CONTROLLED CLOCK OUTPUT). 
5:4 BSM 
Backup Switchover Mode 
(see AUTOMATIC BACKUP SWITCHOVER FUNCTION,  
AUTOMATIC BACKUP SWITCHOVER INTERRUPT FUNCTION and 
TRICKLE CHARGER WITH CHARGE PUMP) 
To read/write from/to the EEPROM, the user has to disable the Backup Switchover 
function by setting the BSM field to 00 or 11 (see routine in  
EEPROM READ/WRITE CONDITIONS) 
00 Switchover Disabled. - Default value on delivery 
01 
Enables the Direct Switching Mode (DSM). 
Switchover when VDD < VBACKUP (PMU selects pin with the greater voltage 
(VDD or VBACKUP)). 
10 
Enables the Level Switching Mode (LSM). 
Switchover when VDD < VTH:LSM (2.0 V) AND VBACKUP > VTH:LSM (2.0 V). 
When VDD < VTH:LSM (2.0 V), PMU is in DSM Mode. 
11 Switchover Disabled. 
3:2 TCR 
Trickle Charger Series Resistance 
(see TRICKLE CHARGER WITH CHARGE PUMP) 
00 TCR 0.6 kΩ - Default value on delivery 
01 TCR 2 kΩ 
10 TCR 7 kΩ 
11 TCR 12 kΩ 
1:0 TCM 
Trickle Charger Mode (see TRICKLE CHARGER WITH CHARGE PUMP) 
00 Trickle Charger off. - Default value on delivery 
01 
TCM 1.75 V 
? In DSM Mode (BSM = 01), VDD voltage is selected. 
? In LSM Mode (BSM = 10), the internal regulated voltage with the 
typical value of 1.75 V is selected (CeraCharge™ mode). (1) 
10 
TCM 3.0 V 
? In LSM Mode (BSM = 10), the internal charge pump voltage with 
the typical value of 3.0 V is selected. (1) 
11 
TCM 4.5 V 
? In LSM Mode (BSM = 10), the internal charge pump voltage with 
the typical value of 4.5 V is selected. (1) 
(1) In LSM Mode (BSM = 10), the TCM voltage levels 1.75 V, 3.0 V or 4.5 V are only generated when VDD > VTH:LSM (maximum 2.2 V).
```

## Page 46

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 46/154 Rev. 1.3 
3.20.2. EEPROM OFFSET REGISTER 
C1h - EEPROM Offset 
This register holds the OFFSET value for aging correction of the frequency and the PORIE and VLIE bits to enable 
interrupt output. 
After a Power up and the first refreshment time t PREFR = ~66 ms, the EEPROM Offset register value is copied from 
the EEPROM to the corresponding RAM mirror. The default value preset on delivery is 00h. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C1h EEPROM Offset R/WP PORIE VLIE OFFSET 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 PORIE 
Power On Reset Interrupt Enable bit 
(see POWER ON RESET INTERRUPT FUNCTION) 
0 No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when a Power On Reset occurs 
or the signal is cancelled on INTÌ…Ì…Ì…Ì…Ì… pin. - Default value on delivery 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when a Power On Reset 
occurs. This setting is retained until the PORF flag is cleared to 0 (no 
automatic cancellation). 
6 VLIE 
Voltage Low Interrupt Enable bit 
(see VOLTAGE LOW INTERRUPT FUNCTION) 
0 No interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when a Voltage Low event 
occurs or the signal is cancelled on INTÌ…Ì…Ì…Ì…Ì… pin. - Default value on delivery 
1 
An interrupt signal is generated on INTÌ…Ì…Ì…Ì…Ì… pin when a Voltage Low event 
occurs. This setting is retained until the VLF flag is cleared to 0 (no 
automatic cancellation). 
5:0 OFFSET -32 to 
+31 
The amount of the effective frequency offset. This is a two's complement 
number with a range of -32 to +31 adjustment steps (maximum correction 
range is roughly ±7.4 ppm). The correction value of one LSB corresponds 
to 1/(32768*128) = 0.2384 ppm. - Default value on delivery is 0 (see 
AGING CORRECTION). 
 
OFFSET Unsigned decimal Signed decimal 
(two’s complement) Offset value in ppm(*) 
011111 31 31 7.391 
011110 30 30 7153 
: : : : 
000001 1 1 0.238 
000000 (default) 0 0 0.000 
111111 63 -1 -0.238 
111110 62 -2 -0.477 
: : : : 
100001 33 -31 -7.391 
100000 32 -32 -7.629 
(*) Calculated with 5 decimal places (1/(32768 × 128) = 0.23842 ppm) 
 The frequency deviation measured on the CLKOUT pin can be compensated by computing the OFFSET value and writing it into the 
 EEPROM Offset register (see AGING CORRECTION).
```

## Page 47

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 47/154 Rev. 1.3 
3.20.3. EEPROM CLKOUT REGISTERS 
The registers EEPROM Clkout 1 and EEPROM Clkout 2 hold the values HFD [12:0], OS and FD that define the 
frequency to be output. After a Power up and the first refreshment time tPREFR = ~66 ms, the EEPROM Clkout 1 and 
EEPROM Clkout 2 values are copied from the EEPROM to the corresponding RAM mirror . 
The programmable square wave output is available at CLKOUT pin. Operation can be activated directly by setting 
NCLKE bit to 0 (EEPROM C0h) or by an interrupt function (CLKF = 1) (see PROGRAMMABLE CLOCK OUTPUT). 
 
 
C2h - EEPROM Clkout 1 
This register holds the lower 8 bits of the HFD value. The default value preset on delivery is 00h (8192 Hz).  
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C2h 
EEPROM 
Clkout 1 R/WP HFD [7:0] 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 HFD [7:0] 00h to 
FFh 
CLKOUT Frequency Selection in HF mode (lower 8 bits). 
See table on next page. 
 
 
C3h - EEPROM Clkout 2 
This register holds the Oscillator Selection bit, the FD value and the upper 5 bits of the HFD value. The default value 
preset on delivery is 00h (XTAL selected, 32.768 kHz). 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C3h 
EEPROM 
Clkout 2 R/WP OS FD HFD [12:8] 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7 OS 
Oscillator Selection (synchronized oscillator change) 
0 XTAL mode is selected. - Default value on delivery 
1 HF mode is selected. 
6:5 FD 00 to 11 CLKOUT Frequency Selection in XTAL mode. See table below. 
4:0 HFD [12:8] 
00000 
to 
11111 
CLKOUT Frequency Selection in HF mode (upper 5 bits). 
See table on next page. 
 
FD value CLKOUT Frequency Selection in XTAL mode STOP bit 
00 32.768 kHz - Default value on delivery No effect 
01 1024 Hz (1) (2) If STOP bit = 1, the clock output is stopped. CLKOUT 
remains HIGH or LOW. (3) 10 64 Hz (1) (2) 
11 1 Hz (1) (2) 
(1) 1024 Hz to 1 Hz clock pulses can be affected by compensation pulses (see TEMPERATURE COMPENSATION and  
  AGING CORRECTION). 
(2) Current period duration of 1024 Hz to 1 Hz clock pulses are affected when writing to the Seconds register or when the ESYN bit is 1 in 
  case of an External Event detection on EVI pin. 
(3) 1024 Hz, 64 Hz and 1 Hz are synchronously turned on and off by the STOP bit.
```

## Page 48

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 48/154 Rev. 1.3 
HFD (13 bits), 8.192 kHz to 67.109 MHz in 8.192 kHz steps: 
HFD [12:0] value HFD in decimal HFD + 1 
CLKOUT Frequency 
Selection in HF mode 
= (HFD + 1) × 8.192 kHz 
STOP bit 
0000000000000 0 1 8.192 kHz 
 - Default value on delivery 
No effect. (1) (2) 
0000000000001 1 2 16.384 kHz 
0000000000010 2 3 24.576 kHz 
: :  : 
1100011001011 6347 6348 52.002816 MHz 
: :  : 
1111111111110 8190 8191 67.100672 MHz 
1111111111111 8191 8192 67.108864 MHz 
(1) Clock pulses from HF mode are not affected by compensation pulses (no TEMPERATURE COMPENSATION and 
  no AGING CORRECTION). 
(2) Current period duration of clock pulses in HF mode are not affected when writing to the Seconds register nor when the ESYN bit is 1 in 
  case of an External Event detection on EVI pin.
```

## Page 49

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 49/154 Rev. 1.3 
3.20.4. EEPROM TEMPERATURE REFERENCE REGISTERS 
The registers EEPROM T Reference 0 and EEPROM TReference 1 hold the  16-bit Temperature Reference value 
TREF in two's complement format that is used to calibrate the readable Temperature Value TEMP in registers 0Eh 
and 0Fh. TREF defines the calibration steps that can be calculated. Each step introduces a deviation of 0.0078125°C. 
The preconfigured (Factory Calibrated) TREF value may be changed by the user  (see TEMPERATURE 
REFERENCE ADJUSTMENT). 
 
 
C4h - EEPROM TReference 0 
This register holds the lower 8 bits of the 16-bit TREF value. The preconfigured (Factory Calibrated) TREF value 
may be changed by the user. After a Power up and the first refreshment time t PREFR = ~66 ms, the EEPROM 
TReference 0 value is copied from the EEPROM to the corresponding RAM mirror. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C4h 
EEPROM 
TReference 0 R/WP TREF [7:0] 
Default value on delivery Preconfigured (Factory Calibrated) 
 
Bit Symbol Value Description 
7:0 TREF [7:0] 00h to 
FFh Lower 8 bits of the TREF value. 
 
 
C5h - EEPROM TReference 1 
This register holds the upper 8 bits of the 16-bit TREF value. The preconfigured (Factory Calibrated) TREF value 
may be changed by the user. After a Power up and the first refreshment time t PREFR = ~66 ms, the EEPROM 
TReference 1 value is copied from the EEPROM to the corresponding RAM mirror. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C5h 
EEPROM 
TReference 1 R/WP TREF [15:8] 
Default value on delivery Preconfigured (Factory Calibrated) 
 
Bit Symbol Value Description 
7:0 TREF [15:8] 00h to 
FFh Upper 8 bits of the TREF value.
```

## Page 50

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 50/154 Rev. 1.3 
3.20.5. EEPROM PASSWORD REGISTERS 
After a Power up and the first refreshment time tPREFR = ~66 ms, the EEPROM Password registers 0 to 3 with the 32-
bit EEPROM Password are copied from the  EEPROM to the corresponding RAM mirror . The default values preset 
on delivery are 00h. 
 
C6h - EEPROM Password 0 
Bit 0 to 7 from 32-bit EEPROM Password. 
EEPW registers (*WP) can be write-protected by password. 
RAM mirror is Write only. Returns 0 when read. EEPROM can be READ when unlocked. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C6h 
EEPROM 
Password 0  *WP EEPW [7:0] 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EEPW [7:0] 00h to 
FFh Bit 0 to 7 from 32-bit EEPROM Password 
 
C7h - EEPROM Password 1 
Bit 8 to 15 from 32-bit EEPROM Password. 
EEPW registers (*WP) can be write-protected by password. 
RAM mirror is Write only. Returns 0 when read. EEPROM can be READ when unlocked.  
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C7h 
EEPROM 
Password 1 *WP EEPW [15:8] 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EEPW [15:8] 00h to 
FFh Bit 8 to 15 from 32-bit EEPROM Password 
 
C8h - EEPROM Password 2 
Bit 16 to 23 from 32-bit EEPROM Password. 
EEPW registers (*WP) can be write-protected by password. 
RAM mirror is Write only. Returns 0 when read. EEPROM can be READ when unlocked.  
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C8h 
EEPROM 
Password 2 *WP EEPW [23:16] 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EEPW [23:16] 00h to 
FFh Bit 16 to 23 from 32-bit EEPROM Password 
 
C9h - EEPROM Password 3 
Bit 24 to 31 from 32-bit EEPROM Password. 
EEPW registers (*WP) can be write-protected by password. 
RAM mirror is Write only. Returns 0 when read. EEPROM can be READ when unlocked.  
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C9h 
EEPROM 
Password 3 *WP EEPW [31:24] 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EEPW [31:24] 00h to 
FFh Bit 24 to 31 from 32-bit EEPROM Password
```

## Page 51

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 51/154 Rev. 1.3 
3.20.6. EEPROM PASSWORD ENABLE REGISTER 
After a Power up and the first refreshment time t PREFR = ~66 ms, the Password Enable value EEPWE is copied from 
the EEPROM to the corresponding RAM mirror. The default value preset on delivery is 00h.  
 
 
CAh - EEPROM Password Enable 
RAM mirror is Write only. Returns 0 when read. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
CAh 
EEPROM 
Password Enable WP EEPWE 
Default value on delivery 0 0 0 0 0 0 0 0 
 
Bit Symbol Value Description 
7:0 EEPWE 
EEPROM Password Enable 
0 to 254 
Password function disabled. 
When writing a value not equal 255, the password function is disabled. 
 - 00h is the default value preset on delivery 
255 
Password function enabled. 
When writing a value of 255, the Password registers (39h to 3Ch) can be 
used to enter the 32-bit Password. 
 
 
3.21. USER EEPROM 
CBh to EAh - User EEPROM 
32 Bytes of User EEPROM for general purpose storage are provided. 
Read: Always readable. Write: Can be write-protected by password. 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
CBh to EAh User EEPROM R/WP 32 Bytes of non-volatile User EEPROM. - Default values on delivery are 00h
```

## Page 52

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 52/154 Rev. 1.3 
3.22. REGISTER RESET VALUES SUMMARY 
Reset values; RAM, Address 00h to 25h: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
00h 100th Seconds R 0 0 0 0 0 0 0 0 
01h Seconds R/WP 0 0 0 0 0 0 0 0 
02h Minutes R/WP 0 0 0 0 0 0 0 0 
03h Hours R/WP 0 0 0 0 0 0 0 0 
04h Weekday R/WP 0 0 0 0 0 0 0 0 
05h Date R/WP 0 0 0 0 0 0 0 1 
06h Month R/WP 0 0 0 0 0 0 0 1 
07h Year R/WP 0 0 0 0 0 0 0 0 
08h Minutes Alarm R/WP 0 0 0 0 0 0 0 0 
09h Hours Alarm R/WP 0 0 0 0 0 0 0 0 
0Ah Date Alarm R/WP 0 0 0 0 0 0 0 0 
0Bh Timer Value 0 R/WP 0 0 0 0 0 0 0 0 
0Ch Timer Value 1 R/WP 0 0 0 0 0 0 0 0 
0Dh Status R/WP 0 0 0 0 0 X 1 0 
0Eh Temperature LSBs R/WP 0h ? Xh 0 1 ? 0 0 0 
0Fh Temperature MSBs R 00h ? XXh 
10h Control 1 R/WP 0 0 0 0 0 0 0 0 
11h Control 2 R/WP 0 0 0 0 0 0 0 0 
12h Control 3 R/WP 0 0 0 0 0 0 0 0 
13h Time Stamp Contr. R/WP 0 0 0 0 0 0 0 0 
14h Clock Int. Mask R/WP 0 0 0 0 0 0 0 0 
15h EVI Control R/WP 0 0 0 0 0 0 0 0 
16h TLow Threshold R/WP 0 0 0 0 0 0 0 0 
17h THigh Threshold R/WP 0 0 0 0 0 0 0 0 
18h TS TLow Count R 0 0 0 0 0 0 0 0 
19h TS TLow Seconds R 0 0 0 0 0 0 0 0 
1Ah TS TLow Minutes R 0 0 0 0 0 0 0 0 
1Bh TS TLow Hours R 0 0 0 0 0 0 0 0 
1Ch TS TLow Date R 0 0 0 0 0 0 0 0 
1Dh TS TLow Month R 0 0 0 0 0 0 0 0 
1Eh TS TLow Year R 0 0 0 0 0 0 0 0 
1Fh TS THigh Count R 0 0 0 0 0 0 0 0 
20h TS THigh 
Seconds R 0 0 0 0 0 0 0 0 
21h TS THigh Minutes R 0 0 0 0 0 0 0 0 
22h TS THigh Hours R 0 0 0 0 0 0 0 0 
23h TS THigh Date R 0 0 0 0 0 0 0 0 
24h TS THigh Month R 0 0 0 0 0 0 0 0 
25h TS THigh Year R 0 0 0 0 0 0 0 0 
X = not defined, or defined under conditions.
```

## Page 53

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 53/154 Rev. 1.3 
Reset values; RAM, Address 26h to FFh: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
26h TS EVI Count R 0 0 0 0 0 0 0 X 
27h TS EVI 100th Secs. R 0 0 0 0 0 0 0 0 
28h TS EVI Seconds R 0 0 0 0 0 0 0 0 
29h TS EVI Minutes R 0 0 0 0 0 0 0 0 
2Ah TS EVI Hours R 0 0 0 0 0 0 0 0 
2Bh TS EVI Date R 0 0 0 0 0 0 0 X 
2Ch TS EVI Month R 0 0 0 0 0 0 0 X 
2Dh TS EVI Year R 0 0 0 0 0 0 0 0 
2Eh to 38h RESERVED Prot. 00h 
39h Password 0 W 0 0 0 0 0 0 0 0 
3Ah Password 1 W 0 0 0 0 0 0 0 0 
3Bh Password 2 W 0 0 0 0 0 0 0 0 
3Ch Password 3 W 0 0 0 0 0 0 0 0 
3Dh EE Address R/WP 1 1 0 0 0 0 0 0 
3Eh EE Data R/WP 0 0 0 0 0 0 0 0 
3Fh EE Command WP 0 0 0 0 0 0 0 0 
40h to 4Fh User RAM 
(16 Bytes) R/WP 00h 
50h to BFh RESERVED Prot. 00h 
CBh to FFh RESERVED Prot. 00h 
X = not defined, or defined under conditions.
```

## Page 54

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 54/154 Rev. 1.3 
Default values on delivery; Configuration EEPROM with RAM mirror, Address C0h to CAh: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
C0h EEPROM PMU R/WP 0 0 0 0 0 0 0 0 
C1h EEPROM Offset R/WP 0 0 0 0 0 0 0 0 
C2h EEPROM 
Clkout 1 R/WP 0 0 0 0 0 0 0 0 
C3h EEPROM 
Clkout 2 R/WP 0 0 0 0 0 0 0 0 
C4h EEPROM 
TReference 0 R/WP Preconfigured (Factory Calibrated) 
C5h EEPROM 
TReference 1 R/WP Preconfigured (Factory Calibrated) 
C6h EEPROM 
Password 0 *WP 0 0 0 0 0 0 0 0 
C7h EEPROM 
Password 1 *WP 0 0 0 0 0 0 0 0 
C8h EEPROM 
Password 2 *WP 0 0 0 0 0 0 0 0 
C9h EEPROM 
Password 3 *WP 0 0 0 0 0 0 0 0 
CAh EEPROM PW 
Enable WP 0 0 0 0 0 0 0 0 
 
 
Default values on delivery; User EEPROM, Address CBh to EAh: 
 
Address Function Conv. Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
CBh to EAh User EEPROM 
(32 Bytes) R/WP 00h
```

## Page 55

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 55/154 Rev. 1.3 
RV-3032-C7 reset values after power on (RAM) and default values on delivery (EEPROM)  sorted by functions: 
 
RAM, reset values: 
Time (hh:mm:ss.00)      =  00:00:00.00  (100th Seconds = read only) 
Date (YY-MM-DD)       =  00-01-01 
Weekday            =  0 
TS TLow Count        =  0        (read only) 
TS TLow Time (hh:mm:ss)   =  00:00:00    (read only) 
TS TLow Date (YY-MM-DD)  =  00-00-00    (read only) 
TS THigh Count        =  0        (read only) 
TS THigh Time (hh:mm:ss)  =  00:00:00    (read only) 
TS THigh Date (YY-MM-DD)  =  00-00-00    (read only) 
TS EVI Count         =  X        (read only) 
(if X = 1, LOW level was detected. Else X = 0) 
TS EVI Time (hh:mm:ss.00)  =  00:00:00.00  (read only) 
TS EVI Date (YY-MM-DD)   =  00-XX-XX   (read only) 
(if XX-XX = 01-01, LOW level was detected. Else XX-XX = 00-00) 
Alarm function         =  disabled, because AE_D = 0 = enabled and 
                  Date Alarm value = 00h = not valid 
Timer function         =  disabled, Timer Clock Frequency = 4096 Hz 
Update function        =  disabled, second update is selected 
Temperature value TEMP   =  000h ? XXXh (read only) 
Temperature Low function   =  disabled, TLow Threshold = 0°C 
Temperature High function   =  disabled, THigh Threshold = 0°C 
External Event function    =  falling edge is regarded as External Event on pin EVI 
Time Stamp Temp. Low    =  disabled, first event is selected 
Time Stamp Temp. High    =  disabled, first event is selected 
Time Stamp Ext. Event    =  first event is selected 
Backup Switchover Interrupt  =  disabled    (for enabling, see EEPROM) 
Interrupts            =  disabled (RAM enabled interrupts), see also Configuration EEPROM 
EEPROM Memory Refresh  =  enabled 
EEbusy status bit       =  1 ? 0 (1 for the time tPREFR = ~66 ms, then it cleared to 0 automatically) 
(read only) 
STOP bit function       =  disabled  (prescaler not stopped) 
ESYN bit function       =  disabled  (no time synchronization by External Event) 
THF Flag            =  0 
TLF Flag            =  0 
UF Flag            =  0 
TF Flag            =  0 
AF Flag            =  0 
EVF Flag            =  X    (X = 1 when LOW level was detected on EVI pin, else X = 0) 
PORF Flag           =  1    (can be cleared by writing 0 to the bit) 
VLF Flag            =  0 
EEF Flag            =  0 
CLKF Flag           =  0 
BSF Flag            =  0 
Interrupt Controlled Clock   =  disabled, no interrupt source selected, CLKOUT delay disabled, 
delay time tI2C:CLK = 1.4 ms selected, no interrupt delay 
Password PW         =  00000000h   (write only) 
EE Address          =  C0h      (points to EEPROM PMU) 
EE Data            =  00h 
EE Command         =  00h       (write only) 
User RAM           =  00h       (16 Bytes)
```

## Page 56

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 56/154 Rev. 1.3 
Configuration EEPROM with RAM mirror, default values on delivery: 
Backup Switchover function  =  disabled    (for Interrupt, see RAM) 
Power On Reset Interrupt   =  disabled 
Voltage Low Interrupt     =  disabled 
Interrupts            =  disabled (EEPROM enabled interrupts), see also RAM 
Trickle Charger Mode     =  disabled, TCR 0.6 kΩ is selected 
OFFSET value         =  0   (6 bits) 
CLKOUT            =  enabled, XTAL mode selected, F = 32.768 kHz 
TREF value          =  Preconfigured Value (16 bits)  (may be changed by the user) 
EEPROM Password EEPW  =  00000000h   (write only) (EEPROM readable when unlocked) 
EEPROM Password Enable  =  disabled    (write only) 
 
 
User EEPROM, default values on delivery: 
User EEPROM (32 Bytes)   =  00h
```

## Page 57

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 57/154 Rev. 1.3 
4. DETAILED FUNCTIONAL DESCRIPTION 
4.1. POWER ON RESET (POR) 
The power on reset (POR) is generated at start -up (see POWER ON RESET INTERRUPT FUNCTION ). All RAM 
registers including the Counter Registers are initialized to their reset values and the Configuration EEPROM registers 
with the RAM mirror registers are set to their preset default values. At power up a refresh of the RAM mirror values 
by the values in the Configuration EEPROM is automatically generated. The time of this first refreshment is t PREFR = 
~66 ms (see REGISTER RESET VALUES SUMMARY). 
The Power On Res et Flag PORF  set to 1 indicates that a V DD startup from below the V POR falling edge threshold 
(TYP 0.95 V) occurred in the VDD Power state, thereby generating a device POR. A PORF value of 1 indicates that 
the time information is corrupted. The value 1 is retained until a 0 is written by the user. 
When PORIE bit ( EEPROM C1h) is set and the PORF flag was cleared beforehand, an interrupt signal on INTÌ…Ì…Ì…Ì…Ì… pin 
can be generated when a Power On Reset occurs (see POWER ON RESET INTERRUPT FUNCTION) 
 
Hint: Resetting the PORF flag is actually not required to get a POR interrupt on INTÌ…Ì…Ì…Ì…Ì… pin as the flag is automatically 
reset when VDD falls below VPOR (TYP 0.95 V) and is set again when VDD crosses the VPOR rising edge threshold (TYP 
1.0 V) at startup. A POR interrupt is carried out in any case.
```

## Page 58

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 58/154 Rev. 1.3 
4.2. AUTOMATIC BACKUP SWITCHOVER FUNCTION 
Basic Hardware Definitions: 
? The RV-3032-C7 has two power supply pins. 
o VDD    is the main power supply input pin. 
o VBACKUP  is the backup power supply input pin. 
? VTH:LSM (typical 2.0 V) is the backup switchover threshold voltage in Level Switching Mode. 
? A debounce logic provides a debounce time tDEB which will filter VDD oscillation when switchover function will 
switch back from V BACKUP to VDD. I2C access is again possible in VDD Power state (and if VDD ≥ 1.4 V) after 
the debounce time tDEB. 
o tDEB MAX = 1 ms, when internal voltage was always above VLOW (typical 1.2 V). VLF = 0. 
o tDEB MAX = 1000 ms, whe n internal voltage was between V LOW (typical 1.2 V) and V POR (maximum 
1.05 V). VLF = 1. See also BACKUP AND RECOVERY AC ELECTRICAL CHARACTERISTICS. 
 
Backup Switchover Modes: 
The RV-3032-C7 has three Backup Switchover Modes. The desired mode can be selected by the BSM field in the 
Configuration EEPROM, see EEPROM PMU REGISTER: 
? BSM = 00  Switchover disabled (default value on delivery), see SWITCHOVER DISABLED. 
? BSM = 01  Direct Switching Mode (DSM), see DIRECT SWITCHING MODE (DSM). 
o If VDD < VBACKUP, switchover occurs from VDD to VBACKUP. 
? BSM = 10  Level Switching Mode (LSM), see LEVEL SWITCHING MODE (LSM). 
o If VBACKUP > VTH:LSM (typical 2.0 V) AND VDD < VTH:LSM (typical 2.0 V), switchover occurs from VDD to 
VBACKUP. 
o If VDD < VTH:LSM (typical 2.0 V), the module is automatically in DSM Mode. 
? BSM = 11  Switchover disabled, see SWITCHOVER DISABLED. 
 
Function Overview: 
When a valid backu p switchover condition occurs (Direct or Level Switching M ode) and the internal power supply 
switches to the VBACKUP voltage (VBACKUP Power state) the following sequence applies: 
? The Backup Switch Flag BSF is set, see AUTOMATIC BACKUP SWITCHOVER INTERRUPT FUNCTION. 
o If BSIE = 1 (register 12h), an interrupt will be generated on INT pin and remains as long as BSF is 
not cleared to 0 (can be cleared when back in VDD Power state). 
o If BSIE = 0, no interrupt will be generated. 
? The I2C-bus interface is automatically disabled (high impedance) and reset. 
? The External Event function is disabled. 
? CLKOUT pin is held LOW. 
For the VBACKUP Power state, the following applies: 
? Temperature sensing and temperature compensation remains active. 
? The interrupt output on INT pin still works. If the interrupt is to be used, a pull-up resistor to VBACKUP is required. 
? Any previously configured interrupt selected in the Clock Interrupt Mask Register (14h) can be used to enable 
the clock out put on CLKOUT pin automatically  (clock output when back in VDD Power state ) (see 
AUTOMATIC BACKUP SWITCHOVER INTERRUPT FUNCTION). 
? The TLow and THigh Time Stamp functions still work (the Time Stamp registers can be read when back in 
VDD Power state). 
? The EVI Time Stamp function does not work, because the EVI function is disabled.
```

## Page 59

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 59/154 Rev. 1.3 
4.2.1. SWITCHOVER DISABLED 
The switchover function is disabled when BSM field (EEPROM C0h) is set to 00 or 11 (BSM = 00 is the default value 
on delivery). 
1. Used when only one power supply is available  (device is always in VDD Power state). The power supply is 
applied on V DD pin and the VBACKUP pin must be tied to V SS with a 10 kΩ resistor.  The Backup Switch Flag 
BSF is always logic 0. 
2. Used when VDD is turned off and VBACKUP is still present and the device must not draw any current from the 
backup source (IBACKUP = 0 nA) . The backup source  on VBACKUP pin is in standby mode until the device is 
powered up again from main supply V DD and a Backup Switchover M ode is selected (see also TYPICAL 
CHARACTERISTICS). 
When the device is first powered up from the backup supply (V BACKUP) but without a main supply (V DD), 
switchover is also disabled and the backup source is automatically in standby mode (IBACKUP = 0 nA). 
 
 
4.2.2. DIRECT SWITCHING MODE (DSM) 
This mode is selected with BSM = 01 (EEPROM C0h). 
? If VDD > VBACKUP the internal power supply is VDD. 
? If VDD < VBACKUP the internal power supply is VBACKUP. 
 
Direct Switching Mode is useful in systems where the supply voltage VDD is known to be higher than VBACKUP: 
? This is the case when charging a rechargeable backup source on V BACKUP (e.g. a supercapacitor) via the 
internal trickle charger with the charge -pump disabled. The charging voltage  reached at VBACKUP is always 
lower than the supply voltage VDD. 
? Do not use if the backup source on VBACKUP is a primary battery with a specified nominal voltage equal to or 
close to the supply voltage VDD. For example, new 3 V lithium coin cell batteries can have a voltage of up to 
3.6 V. With VDD = 3 V or 3.3 V this can lead to unwanted switching. 
See also OPERATING PARAMETERS and TYPICAL CHARACTERISTICS. 
Note that the circuit needs tSWA = 2 ms in the worst case to react when changing from disabled switchover to DSM. 
 
Backup switchover in Direct Switching Mode and Backup Switchover Interrupt enabled, BSIE = 1 (register 12h): 
 
Write operation
VDD PowerPower state
BSF
VBACKUP Power VDD Power
VDD
VBACKUP
INT
enabledI2C access disabled enabled
0 V
tDEB
```

## Page 60

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 60/154 Rev. 1.3 
4.2.3. LEVEL SWITCHING MODE (LSM) 
This mode is selected with BSM = 10 (EEPROM C0h). 
? If VDD > VTH:LSM (typical 2.0 V), the internal power supply is VDD. 
? If VBACKUP > VTH:LSM (typical 2.0 V) AND VDD < VTH:LSM (typical 2.0 V), the internal power supply is VBACKUP. 
? If VDD < VTH:LSM (typical 2.0 V), the module is automatically in DSM Mode (see DIRECT SWITCHING MODE 
(DSM)). 
 
In Level Switching Mode, the power consumption is slightly increased compared to the Direct Switching Mode (DSM) 
because VDD is monitored and compared to the threshold voltage VTH:LSM = typical 2.0 V (typical IDD:LSM = 230 nA). 
See also OPERATING PARAMETERS and TYPICAL CHARACTERISTICS. 
Note that the circuit needs tSWA = 10 ms in the worst case to react when changing from disabled switchover to LSM. 
 
Backup switchover in Level Switching Mode and Backup Switchover Interrupt enabled, BSIE = 1 (register 12h): 
 
(typical 2.0 V)
BSF
VTH:LSM
VDD
VBACKUP
INT
0 V
Example 1: VBACKUP > VDDTH:LSM
VBACKUP
Example 2: VBACKUP < VDDTH:LSM
VDD PowerPower state VBACKUP Power VDD Power
enabledI2C access disabled enabled
tDEB
Write operation
VDD PowerPower state VBACKUP Power VDD Power
enabledI2C access disabled enabled
tDEB
```

## Page 61

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 61/154 Rev. 1.3 
4.3. TRICKLE CHARGER WITH CHARGE PUMP 
The device supporting the VBACKUP pin include a trickle charging circuit with charge pump which allows a battery or 
supercapacitor connected to the VBACKUP pin to be charged: 
? direct from the power supply connected to the VDD pin 
? or by the internal regulated voltage TCM 1.75 V (for TDK’s CeraCharge™) 
? or by one of the internal charge pump voltages TCM 3.0 V or TCM 4.5 V. See figure below. 
In the register EEPROM C0h the Trickle Charger with Charge Pump can be configured by the TCM field (default 
value on delivery is “Trickle Charger of f”) and by the TCR field for selecting a series current limiting resistor (default 
value on delivery is 0.6 kÎ©). A schottky diode, with a typical voltage drop of 0.25 V, is inserted in the charging path.  
The internal charge pump voltages are useful when using a supercapacitor, as it permits to charge the capacitor to 
a higher voltage than VDD. No external components required. 
 
Note that the trickle voltage levels 1.75 V, 3.0 V or 4.5 V are only available when LSM Mode (BSM = 10) is selected 
and VDD > VTH:LSM (maximum 2.2 V). 
 
Trickle Charger with Charge Pump: 
 
0.6 kÎ© 
12 kÎ© 
(LSM)
VDD
2 kÎ© 
7 kÎ© 
VBACKUP
Schottky
diode
TCR
00
01
10
11
6 1
01
10
11
VREG
1.75 V
CP
CP
3.0 V
4.5 V
TCM
01
10
BSM
(VDD)
(DSM)
00
(OFF)
00/11
(OFF)
 
 
 
The trickle charger is disabled when TCM = 00 or when Switchover function is disabled (BSM = 00 or 11) or when 
the device is in VBACKUP Power state. 
 
 
4.4. PROGRAMMABLE CLOCK OUTPUT 
The Oscillator Selection bit OS (EEPROM C3h) can be used to select the XTAL mode or the HF mode. 
In XTAL mode (OS bit = 0) four frequencies can be output on CLKOUT pin, the frequency selection is done in the 
FD field (EEPROM C3h). 
? 32.768 kHz; direct from Xtal oscillator, not temperature compensated and not offset compensated.  
? 1024 Hz, 64 Hz, 1 Hz; divided Xtal oscillator frequencies, always temperature compensated and with aging 
compensation with user programmable EEPROM Offset value (EEPROM C1h).  
In HF mode (OS bit = 1) frequencies from 8192 Hz to 67.109 MHz in 8192 Hz steps can be output on CLKOUT pin, 
the frequency selection is done in the HFD field (EEPROM C2h and C3h). 
? The frequencies are not temperature compensated and not offset compensated.  
 
The frequency output can be controlled directly via the I 2C-bus interface comman ds (normal operation) or can be 
interrupt driven to allow waking up an external system by supplying a clock. 
CLKOUT is tied to VSS in VBACKUP Power state independent of the CLKOUT configuration settings.  
 
After POR, and if the default values on delivery in the Configuration EEPROM with RAM mirror have not changed, 
the 32.768 kHz frequency is output to CLKOUT pin since FD = 00, OS = 0 (EEPROM C3h) and NCLKE = 0 (EEPROM 
C0h). To customize these POR values, the user can change the values in the Configuration E EPROM. 
 
See also EEPROM READ/WRITE.
```

## Page 62

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 62/154 Rev. 1.3 
4.4.1. XTAL CLKOUT FREQUENCY SELECTION 
A programmable XTAL square wave is available at pin CLKOUT  when OS = 0 (EEPROM C3h) . Operation is 
controlled by the FD field (EEPROM C3h). Frequencies fro m 32.768 kHz (default value on delivery) to 1 Hz can be 
generated for use as a system clock, microcontroller clock, input to a charge pump, or for calibration of the crystal 
oscillator. 
 
Pin CLKOUT is a push-pull output that is enabled at power on (default value on delivery). CLKOUT can be disabled 
by setting NCLKE bit to 1  when not enable by an Interrupt function (CLKIE = 0 and CLKF = 0). When disabled, the 
CLKOUT pin is LOW. 
 
The STOP bit function can affect the CLKOUT signal depending on the selected frequency. When STOP = 1, the 
clock output of 1024 Hz, 64 Hz or 1 Hz are stopped (for more details, STOP BIT FUNCTION). 
When writing to the Seconds register or when the ESYN bit is 1 in case of an External Event detection on EVI pin 
the current period duration of 1024 Hz to 1 Hz clock pulses are affected.  
 
XTAL CLKOUT Frequency Selection: 
FD value CLKOUT Frequency Selection in XTAL mode STOP bit 
00 32.768 kHz - Default value on delivery No effect 
01 1024 Hz (1) (2) If STOP bit = 1, the clock output is stopped. CLKOUT 
remains HIGH or LOW. (3) 10 64 Hz (1) (2) 
11 1 Hz (1) (2) 
(1) 1024 Hz to 1 Hz clock pulses can be affected by compensation pulses (see TEMPERATURE COMPENSATION and  
  AGING CORRECTION). 
(2) Current period duration of 1024 Hz to 1 Hz clock pulses are affected when writing to the Seconds register or when the ESYN bit is 1 in 
  case of an External Event detection on EVI pin. 
(3) 1024 Hz, 64 Hz and 1 Hz are synchronously turned on and off by the STOP bit. 
 
 
4.4.2. HF CLKOUT FREQUENCY SELECTION 
A programmable HF square wave is available at pin CLKOUT when OS = 1 (EEPROM C3h). Operation is controlled 
by the HFD field (EEPROM C2h and C3h).  Frequencies from 8192 Hz to 67.109 MHz in 8192 Hz steps can be 
generated for use as a system clock, microcontroller clock or for input to a charge pump. 
 
Pin CLKOUT is a push-pull output that is enabled at power on (default value on delivery). CLKOUT can be disabled 
by setting NCLKE bit to 1 when not enable by an Interrupt function (CLKIE = 0 and CLKF = 0). When disabl ed, the 
CLKOUT pin is LOW. 
 
HF CLKOUT Frequency Selection: 
HFD [12:0] value HFD in decimal HFD + 1 
CLKOUT Frequency 
Selection in HF mode 
= (HFD + 1) × 8.192 kHz 
STOP bit 
0000000000000 0 1 8.192 kHz 
 - Default value on delivery 
No effect. (1) (2) 
0000000000001 1 2 16.384 kHz 
0000000000010 2 3 24.576 kHz 
: :  : 
1100011001011 6347 6348 52.002816 MHz 
: :  : 
1111111111110 8190 8191 67.100672 MHz 
1111111111111 8191 8192 67.108864 MHz 
(1) Clock pulses from HF mode are not affected by compensation pulses (no TEMPERATURE COMPENSATION and 
  no AGING CORRECTION). 
(2) Current period duration of clock pulses in HF mode are not affected when writing to the Seconds register nor when the ESYN bit is 1 in 
  case of an External Event detection on EVI pin.
```

## Page 63

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 63/154 Rev. 1.3 
4.4.3. CLKOUT FREQUENCY TRANSITIONS 
Two applications can be considered:  On the one hand the switching on and off of a preselected frequency, on the 
other hand the change of frequency when CLKOUT is enabled. 
 
CLKOUT on/off (FD, HFD and OS unchanged): 
? With NCLKE bit. Synchronous on/off. 
o FD = 1024 Hz, turn-off delay time of between 0 and one extra period (967 µs) 
o FD = 32768 Hz, turn-on delay time of about 600 µs, and turn-off delay time of about 400 µs 
o HFD frequencies have a turn-on delay time of 2.5 ms, and a turn-off delay time of 400 µs 
? Or by CLKF flag. Synchronous on/off. Same behavior as with bit NCLKE. 
 
CLKOUT frequency change with OS (FD and HFD unchanged, CLKOUT on): 
? FD ? HFD. Synchronous change. About 350 µs old frequency, 2.3 ms CLKOUT = LOW, then new frequency. 
? HFD ? FD. Synchronous change. About 300 µs old frequency, 650 µs CLKOUT = LOW, then LOW until 
next positive edge of the new frequency. 
Hint: Do not change the old frequency at the same time when changing OS bit (register EEPROM C3h). 
 
CLKOUT frequency change with FD or HFD (OS unchanged, CLKOUT on): 
? With FD (XTAL mode). Immediate frequency change. 
? With HFD (HF mode). Immediate frequency change. Not recommended as the new frequency is not stable 
for the first few milliseconds. The better way is to stop the clock, change the frequ ency, and start the clock 
again (for example, with bit NCLKE). 
 
 
4.4.4. NORMAL CLOCK OUTPUT 
Normal clock output is controlled b y the NCLKE bit (EEPROM C0h). When NCLKE is set to 0 (default), the square 
wave output is enabled on the CLKOUT pin. When NCLKE bit is set to 1, the CLKOUT pin is LOW, if not enabled by 
the interrupt driven clock output (CLKF = 0). 
 
 
4.4.5. INTERRUPT CONTROLLED CLOCK OUTPUT 
To use interrupt controlled clock output, NCLKE (EEPROM C0h) has to be set to 1 (CLKOUT not directly enabled). 
When CLKIE bit (11h) is set to 1 the occurrence of the interrupt selected in the  Clock Interrupt Mask Register 14h 
(CEIE, CAIE, CTIE, CUIE, CTHIE or CTLIE)  allows the flag CLKF to be set and  the square wave output on the 
CLKOUT pin. This function allows waking up an external system (MCU) by outputting a clock.  
Writing 0 to CLKIE will disable new interrupts from driving frequencies on CLKOUT, but if there is already an active 
interrupt driven frequency output (CLKF flag is set), the active frequency output will not be stopped. When CLKF flag 
is cleared, the CLKOUT pin is LOW. 
 
? An Interrupt Delay after CLKOUT-on can be enabled with bit INTDE (14h). Used when waking up an MCU. 
See INTERRUPT DELAY AFTER CLKOUT ON. 
? A CLKOUT switch off delay after I2C STOP can be selected and enabled by bits CLKD and CLKDE (registers 
14h and 15h). Used if the MCU wants to put itself into sleep mode.  See CLKOUT OFF DELAY AFTER I 2C 
STOP. 
Caution, it is possible that the MCU can put  into sleep mode without a valid interrupt function enabled that 
could wake it up again.
```

## Page 64

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 64/154 Rev. 1.3 
4.4.6. INTERRUPT DELAY AFTER CLKOUT ON 
When using INTERRUPT CONTROLLED C LOCK OUTPUT an Interrupt Delay after CLKOUT O n can be enabled 
with bit INTDE (14h). Used when waking up an MCU. 
Applicable only when NCLKE bit (EEPROM C0h) is set to 1 (CLKOUT not directly enabled) and fo r all activated 
interrupt functions with the appropriate clock output bit in register 14h enabled (CEIE, CAIE, CTIE, CUIE, CTHIE or 
CTLIE). 
? When INTDE = 0, no delay is added (default value). 
? When INTDE = 1, the delay time tCLK:INT of 1/256 seconds to 3/512 seconds ≈ 3.9 ms to 5.9 ms is added. 
 
Note that no delay can be created with the Periodic Countdown Timer Interrupt function (CTIE = 1)  when TD = 00 
(4096 Hz) is selected. With the other settings TD = 01, 10, 11 (64 Hz, 1 Hz, 1/60 Hz) the delay can be applied. 
 
 
4.4.7. CLKOUT OFF DELAY AFTER I2C STOP 
When using INTERRUPT CONTROLLED C LOCK OUTPUT  a CLKOUT switch off delay after I 2C STOP can be 
selected and enabled by bits CLKD and CLKDE (registers 14h and 15h). Used if the MCU wants to put itself into 
sleep mode. 
Caution, it is possible that the MCU can be put into sleep mode without a valid interrupt function enabled that could 
wake it up again. 
 
CLKD bit is used to select one of two delay values: 
? When CLKD = 0, typical delay time tI2C:CLK = 1.4 ms (default value). 
? When CLKD = 1, typical delay time tI2C:CLK = 75 ms. 
 
To enable the CLKOUT off Delay after I2C STOP the CLKDE bit has to be set to 1. 
? When CLKDE = 0, no delay (default value). 
? When CLKDE = 1, delay is enabled. The delay time is according to bit CLKD.
```

## Page 65

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 65/154 Rev. 1.3 
4.4.8. CLOCK OUTPUT SCHEME 
Clock output scheme: 
 
0
CLKDE
1
0
1
CLKD
0
1
TLI
(1)
OR
 1
0
1
THI
(1)
0
1
UI
(1)
0
1
TI
(1)
0
1
AI
(1)
0
1
EI
(1)
0
CLKIE
1
CLOCK FLAG
CLKF
SET
CLEAR
to interface:
read CLKF
FD
(2)  32.768 kHz
1024 Hz
64 Hz
1 Hz
00
01
10
11
0
NCLKE
1
OR
 1
CLEAR
CLKOUT off 
Delay after 
I2C STOP
OS
0
1HFD
(2)  8.192 kHz
16.384 kHz
:
67.109 MHz
0
1
:
8191
from interface:
clear CLKF
1.4 ms
75 ms
AND
& CLKOUT
VBACKUP
Power state
(3)
0
1
Synchronous
change
Synchronous
on/off
Synchronous
on/off
CTLIE
CTHIE
CUIE
CTIE
CAIE
CEIE
(2)
(2)
 
 
(1) See INTERRUPT SCHEME. 
(2) Default values on delivery for FD (EEPROM C3h), HFD (EEPROM C2h and C3h), OS (EEPROM C3h) 
and NCLKE (EEPROM C0h). 
(3) When a frequency is selected and the RTC module is in VBACKUP Power state, CLKOUT pin is LOW. 
When again in VDD Power state, CLKOUT pin outputs the frequency.
```

## Page 66

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 66/154 Rev. 1.3 
4.5. SETTING AND READING THE TIME 
Data flow and data dependencies starting from the 1 Hz clock tick: 
 
MINUTES
HOURS
LEAP YEAR
CALCULATION
MONTH
YEAR
1 Hz tick
DATE WEEKDAY
SECONDS
100TH SECS.
LOGIC CIRCUIT
2048 Hz
 
 
 
During an I2C read/write access to any RTC register that takes less than 950 milliseconds, the time counters (clock 
and calendar registers 01h to 07h, but not the 100 th Seconds register 00h) of the RV-3032-C7 are blocked. During 
this time the clock counter increment  (1 Hz tick) is inhibited to allow coherent data values. One counter increment 
(maximum one 1 Hz tick)  occurring during inhibition time is memorized and will be realized after the I 2C STOP 
condition. 
Exception: If during the inhibition time 0 and then 1 is written to the STOP bit or in case of an External Event when 
ESYN = 1 or a value is written to the Seconds register a possible currently memorized 1 Hz update is reset and the 
prescaler frequencies from 4096 Hz to 1 Hz are reset. Resetting the prescaler will have an influence on the length of 
the current clock period on all subsequent peripherals (clock and calendar, XTAL CLKOUT, timer clock, update timer 
clock, temperature sensing and EVI input filter), (see TIME SYNCHRONIZATION). 
 
When I2C read/write access has b een terminated within 950 mi lliseconds (t < 950  ms), the time counters are 
unblocked with the I2C STOP condition and a pending request to increment the time counters tha t occurred during 
read or write access is correctly applied. Maximum one 1 Hz tick can be handled (see following Figure). 
 
Access time for read/write operations: 
 
SLAVE ADDRESS DATA DATA
t < 950 ms
START STOP
 
 
 
Because of this method, it is very important to make a read or write access in one go, that is, setting or reading  
Seconds register through to Y ear register should be made in one single access. Failing to comply with this method 
could result in the time becoming corrupted. 
 
Hint: If the 100 th Seconds register is read as part of the counter burst (and the 100 th Seconds value is either 00 or 
99), all time counters sho uld be read two or three times until it is verified that two readings are the same value and 
guaranteed to be correct.
```

## Page 67

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 67/154 Rev. 1.3 
4.5.1. SETTING THE TIME 
During an I2C read/write access to any RTC register with an access time of less than 950 ms, the time counters (but 
not the 100th Seconds register) are blocked. After I2C STOP condition a possibly memorized 1 Hz tick is realized. 
Note that when writing to the Seconds register the 100th Seconds register value is cleared to 00. 
 
Advantage of register blocking: 
? Prevents faulty writing to the clock and calendar registers during an I2C write access (no incrementing of time 
registers during the write access). 
? After writing, one memorized 1 Hz tick is handled. Clock and calendar are updated. 
? No reading is needed for control. The written data are coherent. 
 
If the I2C write access takes longer than 950 ms the I2C bus interface is reset by the internal bus timeout function. In 
this case the previous time counter values are maintained, the pending 1 Hz tick is realized and the clock counter 
increment (1 Hz tick) continues to operate normally. Restarting of communications begins with transfer of the START 
condition again. 
The I2C auto increment Address Pointer is not reset by the I2C STOP condition nor by the internal stop forced after 
timeout. 
 
Two methods for setting the time can be distinguished: 
1. Setting the time registers including Seconds register. Writing to the Seconds register resets a possible 
currently memorized 1 Hz update and resets the prescaler frequencies from 4096 Hz to 1 Hz 
(synchronization). 
2. Setting the time registers without Seconds register. A possibly memorized 1 Hz tick during write access will 
be realized. Old synchronicity persists. 
 
Hint: Instead of writing to the Seconds register t o synchronize the time counters the STOP Bit function or the ESYN 
Bit function can be applied. Both functions do not change the value in the Seconds regist er, but they also reset the 
prescaler frequencies from 4096 Hz to 1 Hz (see TIME SYNCHRONIZATION). 
 
 
4.5.2. READING THE TIME 
During an I2C read/write access to any RTC register with an access time of less than 950 ms, the time counters (but 
not the 100th Seconds register) are blocked. After I2C STOP condition a possibly memorized 1 Hz tick is realized. 
 
Advantage of register blocking: 
? Prevents faulty reading of the clock and calendar registers during an I2C read access (no incrementing of 
time registers during the read access). 
? After reading, one memorized 1 Hz tick is handled. Clock and calendar are updated. 
? No second reading is needed for control. The read data are coherent. 
 
If the I2C read access takes longer than 950 ms the I 2C bus interface is reset by the internal bus timeout function. 
In this case, all subsequently read data has the value FFh, the pending 1 Hz tick is realized and the clock counter 
increment (1 Hz tick) continues to operate normally. Restarting of communications begins with transfer of th e 
START condition again. 
The I2C auto increment Address Pointer is not reset by the I2C STOP condition nor by the internal stop forced after 
timeout.
```

## Page 68

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 68/154 Rev. 1.3 
4.6. EEPROM READ/WRITE 
The following registers and bits are related to the EEPROM read/write functions:  
 
? EE Address Register (3Dh) (see EEPROM MEMORY CONTROL REGISTERS) 
? EE Data Register (3Eh) (see EEPROM MEMORY CONTROL REGISTERS) 
? EE Command Register (3Fh) (see EEPROM MEMORY CONTROL REGISTERS) 
? EEF flag and EEbusy status bit (see TEMPERATURE REGISTERS, 0Eh - Temperature LSBs) 
? EERD bit (see CONTROL REGISTERS, 10h - Control 1) 
 
 
4.6.1. POR REFRESH (ALL CONFIGURATION EEPROM ? RAM) 
Automatic read of all Configuration EEPROM registers at Power On Reset (POR): 
? At power up a refresh of the Configuration RAM mirror values by the values in the Configuration EEPROM 
is automatically generated (see REGISTER RESET VALUES SUMMARY). 
? The time of this first refreshment is tPREFR = ~66 ms. 
? The EEbusy bit in the register Temperature LSBs (0Eh) can be used to monitor the status of the refreshment. 
 
 
4.6.2. AUTOMATIC REFRESH (ALL CONFIGURATION EEPROM ? RAM) 
Read all Configuration EEPROM registers automatically: 
? To keep the integrity of the configuration data , all data of the Configuration RAM are refreshed by the data 
in the Configuration EEPROM each 24 hours, at date increment (at the beginning of the last second before 
midnight). 
? The time of this automatic refreshment is tAREFR = ~1.4 ms. 
? Refresh is only active when RV-3032-C7 is not in VBACKUP Power state  and not disabled by EERD 
(EEPROM Memory Refresh Disable) bit. 
? Hint: It is not always necessary/meaningful to turn off the auto -refresh (EERD = 1) before an EEPROM 
access. e.g. if the current RTC time is 01 hour, etc. 
 
 
4.6.3. UPDATE (ALL CONFIGURATION RAM ? EEPROM) 
Write to all Configuration EEPROM registers (see also USE OF THE CONFIGURATION REGISTERS): 
? Before starting to change the configuration stored in the EEPROM, the auto refresh of the registers from the 
EEPROM has to be disabled by writing 1 into the EERD control bit in the Control 1 register. 
? Then the new configuration can be written into the configuration RAM registers, when the whole new 
configuration is in the registers, writing the command 11h into the register EECMD will start the copy of the 
configuration into the EEPROM. 
? The time of the update is tUPDATE = ~46 ms. 
? When the transfer is finished (EEbusy = 0), the user can enable again the auto refresh of the r egisters by 
writing 0 into the EERD bit. 
? The EEF flag in the register Temperature LSBs (0Eh) can be used for EEPROM write access failure 
detection. 
 
 
4.6.4. REFRESH (ALL CONFIGURATION EEPROM ? RAM) 
Read all Configuration EEPROM registers: 
? Before starting to read the configuration stored in the EEPROM, the auto refresh of the registers from the 
EEPROM has to be disabled by writing 1 into the EERD control bit in the Control 1 register. 
? Then the actual configuration can be read from the Config uration EEPROM registers, writing the command 
12h into the register EECMD will start the copy of the configuration into the RAM. 
? The time of this controlled refreshment is tREFR = ~1.4 ms. 
? Functions become active as soon as the RAM bytes are written. 
? When the transfer is finished (EEbusy = 0), the user can enable again the auto refresh of the registers  by 
writing 0 into the EERD bit.
```

## Page 69

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 69/154 Rev. 1.3 
4.6.5. WRITE TO ONE EEPROM BYTE (EEDATA (RAM) ? EEPROM) 
Write to one EEPROM byte of the Configuration EEPROM or User EEPROM regis ters: 
? Before starting to change data stored in the EEPROM, the auto refresh of the registers from the EEPROM 
has to be disabled by writing 1 into the EERD control bit in the Control 1 register. 
? In order to write a single byte to the EEPROM, the address to which the data must be written is entered in 
the EEADDR register and the data to be written is entered in the EEDATA register , then the command 21h 
is written in the EECMD register to start the EEPROM write. 
? The time to write to one EEPROM byte is tWRITE = ~4.8 ms. 
? When the transfer is finished (EEbusy = 0), the user can enable again the auto refresh of the registers  by 
writing 0 into the EERD bit. 
? The EEF flag in the register Temperature LSBs (0Eh) can be used for EEPROM write access failure  
detection. 
 
 
4.6.6. READ ONE EEPROM BYTE (EEPROM ? EEDATA (RAM)) 
Read one EEPROM byte from Configuration EEPROM or User EEPROM registers:  
? Before starting to read a byte in the EEPROM, the auto refresh of the registers from the EEPROM has to be 
disabled by writing 1 into the EERD control bit in the Control 1 register. 
? In order to read a single byte from the EEPROM, the address to be read is entered in the EEADDR register, 
then the command 22 h is written in the EECMD register  and the resulting byte value can be read from the 
EEDATA register. 
? The time to read one EEPROM byte is tREAD = ~1.1 ms. 
? When the transfer is finished (EEbusy = 0), the user can enable again the auto refresh of the registers by 
writing 0 into the EERD bit.
```

## Page 70

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 70/154 Rev. 1.3 
4.6.7. EEBUSY BIT 
The set EEbusy status bit  (bit 2 in the Temperature LSBs register, 0Eh) indicates that the EEPROM is currently 
handling a read or write request and will ignore any further commands until the current one is finished. At power up 
a refresh is automatically generated. The time of this first refr eshment is tPREFR = ~66 ms . After the refreshment is 
finished; EEbusy is cleared to 0 automatically. The cleared EEbusy status bit indicates that the EEPROM transfer is 
finished. 
To prevent access collision between the internal automatic EEPROM refresh cycle (EERD = 0)  and external 
EEPROM read/write access through interface the following procedures have to be applied. 
 
? Set EERD = 1 Automatic EEPROM Refresh needs to be disabled before EEPROM access.  
? Check for EEbusy = 0 Access EEPROM only if not busy. 
? Write EEPROM For example, wait 10 ms after each written EEPROM register before checking for  
 EEbusy = 0 to allow internal data transfer (for Read EEPROM, wait, e.g. 1 ms). 
? Clear EERD = 0 It is recommended to enable Automatic EEPROM Refresh at the end of read/write 
 access. 
 
EEbusy bit flowcharts (examles): 
Read One EEPROM or Refresh All (1) Write to One EEPROM or Update All (2) 
 
  
YES
YES
YES
NO
NO
NO
 EEPROM
is busy?
Read One EEPROM
or Refresh All
Disable
automatic refresh EERD = 1
EEbusy = 1?
EEPROM
Read/Refresh access
is permitted
Next read?
Enable
automatic refresh EERD = 0
Wait
1 ms
Wait 1 ms
to allow internal
EEPROM read
 EEPROM
is busy?
Wait until pervious
read cycle is finished
EEbusy = 1?  
 
 
  
YES
YES
YES
NO
NO
NO
 EEPROM
is busy?
Write One EEPROM
or Update All
Disable
automatic refresh EERD = 1
EEbusy = 1?
EEPROM
Write/Update access
is permitted
Next write?
Enable
automatic refresh EERD = 0
Wait
10 ms
Wait 10 ms
to allow internal
EEPROM write
 EEPROM
is busy?
Wait until pervious
write cycle is finished
EEbusy = 1?  
 
(1) See READ ONE EEPROM BYTE (EEPROM ? EEDATA (RAM)) 
and REFRESH (ALL CONFIGURATION EEPROM ? RAM) 
(2) See WRITE TO ONE EEPROM BYTE (EEDATA (RAM) ? 
EEPROM) and UPDATE (ALL CONFIGURATION RAM ? 
EEPROM) 
 
Note: A minimum power supply voltage of VDD:WRITE = 1.6 V during the whole EEPROM write procedure is required; 
i.e. until EEbusy = 0.
```

## Page 71

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 71/154 Rev. 1.3 
4.6.8. EEF FLAG 
The set EEF flag (bit 3 in the Temperature LSBs register , 0Eh) indicates that the EEPROM write access has failed 
because power supply voltage  VDD has dropped below V DD:EEF. The maximum VDD:EEF = 1.5 volts. The value 1 is 
retained until a 0 is written by the user. 
 
Note that this flag does not provide information about the EEPROM read access. 
 
 
4.6.9. EEPROM READ/WRITE CONDITIONS 
During a read/write of the EEPROM, if the V DD supply drops, the device will continue to operate and communicate 
until a switchover to V BACKUP occurs (in DSM or LSM Mode). It is not recommended to operate during this time and 
all I2C communication should be halted as soon as VDD failure is detected. 
During the time that data is being  written to the EEPROM, V DD should remain above the minimum write voltage 
VDD:WRITE = 1.6 V. If at any time V DD drops below this vo ltage, the data written to the device get corrupted  (EEF set 
when VDD < VDD:EEF). 
To write to the EEPROM, the backup switchover circuit must switch back to the main power supply V DD. See also 
AUTOMATIC BACKUP SWITCHOVER FUNCTION.
```

## Page 72

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 72/154 Rev. 1.3 
4.6.10. USE OF THE CONFIGURATION REGISTERS 
The best practice method to use the Configuration EEPROM with RAM mirror registers at addresses C0h to CAh is 
to make all Configuration settings in the RAM mirror first and then to update all Configuration EEPROMs by the 
Update EEPROM command. 
 
Update all Configuration EEPROMs: 
 
 
C0
CA
EEPW 2
EEPROM TRef. 1
EEPROM PMU
EEPROM Offset
EEPROM Clkout 1
EEPROM Clkout 2
EEPROM TRef. 0
EEPW 0
EEPW 1
EEPW 3
EEPWE
RAM mirror
EEPROM
Configuration EEPROM
with RAM mirror
 
EEPROM Offset
EEPROM Clkout 2
EEPROM PMU
EEPROM Clkout 1
C0
EEPROM TRef. 0
EEPROM TRef. 1
EEPW 0
EEPW 1
EEPW 2
EEPW 3
EEPWE CA  
 
 
The method, how to enable /disable write protection and how to change the reference password can be found in 
section USER PROGRAMMABLE PASSWORD (Configuration Registers C6h to CAh). 
 
Configuration Registers C0h to C5h: 
? EEPROM PMU REGISTER, C0h - EEPROM PMU 
? EEPROM OFFSET REGISTER, C1h - EEPROM Offset 
? EEPROM CLKOUT REGISTERS, C2h - EEPROM Clkout 1 
? EEPROM CLKOUT REGISTERS, C3h - EEPROM Clkout 2 
? EEPROM TEMPERATURE REFERENCE REGISTERS, C4h - EEPROM TReference 0 
? EEPROM TEMPERATURE REFERENCE REGISTERS, C5h - EEPROM TReference 1 
 
Edit the Configuration settings (example, when write protection is enabled (EEPWE = 255)): 
1. Enter the correct password PW (PW = EEPW) to unlock write protection 
2. Disable automatic refresh by setting EERD = 1 
3. Edit Configuration settings in registers C0h to C5h (RAM) 
4. Update EEPROM (all Configuration RAM ? EEPROM) by setting EECMD = 11h 
5. Enable automatic refresh by setting EERD = 0 
6. Enter an incorrect password PW (PW ≠ EEPW) to lock the device 
 
 
Note: RAM mirror of the Configuration registers defines the active zone . By writing only to the EEPROM, the 
configurations are not active. The configurations are activated as soon as a refresh occurs (POR refresh, Automatic 
refresh or Refresh by software). 
 
Note: To perform certain tests, it is sufficient to use only the RAM mirror. But the new, changed configurations are 
lost as soon as a refresh occurs (POR refresh, Automatic refresh or Refresh by software).
```

## Page 73

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 73/154 Rev. 1.3 
4.7. INTERRUPT OUTPUT 
The interrupt output pin INTÌ…Ì…Ì…Ì…Ì… can be triggered by nine different functions: 
 
? PERIODIC COUNTDOWN TIMER INTERRUPT FUNCTION 
? PERIODIC TIME UPDATE INTERRUPT FUNCTION 
? ALARM INTERRUPT FUNCTION 
? EXTERNAL EVENT INTERRUPT FUNCTION 
? TEMPERATURE LOW INTERRUPT FUNCTION 
? TEMPERATURE HIGH INTERRUPT FUNCTION 
? AUTOMATIC BACKUP SWITCHOVER INTERRUPT FUNCTION 
? POWER ON RESET INTERRUPT FUNCTION 
? VOLTAGE LOW INTERRUPT FUNCTION 
 
Note that the interrupt output pin INTÌ…Ì…Ì…Ì…Ì… also works in VBACKUP Power state  (except for the External Event function 
which is disabled). 
 
 
4.7.1. SERVICING INTERRUPTS 
The INTÌ…Ì…Ì…Ì…Ì… pin can indicate nine types of interrupts. It outputs the logic OR operation result of these interrupt outputs. 
When an interrupt is detected (when INTÌ…Ì…Ì…Ì…Ì… pin produces a negative pulse or is at low level), the TF, UF, AF, TLF, THF 
EVF, VLF, BSF and PORF flags can be read to determine which interrupt event has occurred. 
To keep INTÌ…Ì…Ì…Ì…Ì… pin from changing to low level, clear the  TIE, UIE, AIE, EIE, TLIE, THIE, BSIE, PORIE and VLIE bits. 
Bits PORIE and VLIE are located in the EEPROM register C1h. To check whether an event has occurred without 
outputting any interrupts via the INTÌ…Ì…Ì…Ì…Ì… pin, software can read the TF, UF, AF, EVF, TLF, THF, BSF, PORF and VLF 
interrupt flags (polling).
```

## Page 74

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 74/154 Rev. 1.3 
4.7.2. INTERRUPT SCHEME 
Interrupt Scheme (part 1/3): 
 
MINUTES ALARM
MINUTES TIME
=
HOURS ALARM
HOURS TIME
=
DATE ALARM
DATE
=
check now signal
ALARM CONTROL
(1)
from interface:
clear AF
to interface:
read AF
0
AIE
1
0
AE_M
1
0
AE_H
1
0
AE_D
1
AI (3)
0
TIE
1
tRTN1
TI (3)
COUNTDOWN
COUNTER
TIMER FLAG
TF
SET
CLEAR PULSE
GENERATOR 1
TRIGGER
CLEAR
TE, TD and
Reg. 0Bh, 0Ch
TD
to interface:
read TF
0
UIE
1
tRTN2
UI (3)
UPDATE
GENERATOR
UPDATE FLAG
UF
SET
CLEAR PULSE
GENERATOR 2
TRIGGER
CLEAR
USEL
USEL
to interface:
read UF
ALARM FLAG
AF
SET
CLEAR
TD,
INTDE, CTIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON 
0
UIE
1
INTDE, CUIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON 
INTDE, CAIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON 
OR
 1
INT
(2)
clear THF
and TLF
clear THF
and TLF
clear THF
and TLF
from interface:
clear UF
from interface:
clear TF
 
 
(1) Only when all enabled alarm settings are matching. It is only on increment to a matched case that the 
Alarm Flag AF is set. 
When AE_D = 0 = enabled (default) and Date Alarm value = 00h = not valid (default) the Alarm function 
is deactivated (AF flag is never set). 
Note that if all AE_x = 1, there is no match, but an alarm event occurs every minute. 
(2) When bits TIE, UIE, AIE, EIE, TLIE, THIE, BSIE, PORIE (EEPROM C1h) and VLIE (EEPROM C1h) are 
disabled, pin INTÌ…Ì…Ì…Ì…Ì… remains high impedance. 
(3) See CLOCK OUTPUT SCHEME.
```

## Page 75

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 75/154 Rev. 1.3 
Interrupt Scheme (part 2/3): 
 
clear THF
and TLF
THIGH FLAG
    THF (4)
TLOW FLAG
    TLF (4)
EXT. EVENT
FUNCTION
EVENT FLAG
EVF
SET
0
EIE
1
EHL, ET
to interface:
read EVF
EI (3)
INTDE, CEIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON 
SET
0
TLIE
1
to interface:
read TLF
TLI (3)
INTDE, CTLIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON CLEAR
TLR clear TS TLow
registers
TLT
TEMP [11:4]
<
0
TLE
1
check once
per second
SET
to interface:
read THF
THI (3)
INTDE, CTHIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON CLEAR
THR clear TS THigh
registers
TS THigh
scheme
THT
TEMP [11:4]
>
0
THE
1
check once
per second
0
THIE
1
OR
 1
INT
(2)
TS EVI
scheme
TS TLow
scheme
from interface:
clear EVF
CLEAR
CLEARfrom interface:
clear THF
clear TLF
CLEARfrom interface:
clear TLF
clear THF
AND
&VBACKUP
Power state
(5)
EVI
 
 
(2) When bits TIE, UIE, AIE, EIE, TLIE, THIE, BSIE, PORIE (EEPROM C1h) and VLIE (EEPROM C1h) are 
disabled, pin INTÌ…Ì…Ì…Ì…Ì… remains high impedance. 
(3) See CLOCK OUTPUT SCHEME. 
(4) Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 
0s or 1s). 
(5) In VBACKUP Power state, the External Event function is disabled. 
 
See also Time Stamp scheme in sections TIME STAMP EVI SCHEME, TIME STAMP TLOW SCHEME and 
TIME STAMP THIGH SCHEME.
```

## Page 76

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 76/154 Rev. 1.3 
Interrupt Scheme (part 3/3): 
 
(6)
AUTOMATIC BACKUP 
SWITCHOVER FUNCTION
BSM
BACKUP FLAG
BSF
SET
0
1
BSIE
to interface:
read PORF
POR FLAG
PORF
SET
0
1
PORIE
to interface:
read VLF
VLOW FLAG
VLF
SET
0
1
VLIE
to interface:
read BSF
OR
 1
VLOW 
DETECTION
(TYP 1.2 V)
VPOR
DETECTION
(TYP 0.9 V)
CLEAR
VDD
VBACKUP PMU
CLEAR
CLEAR
check once
per second
clear THF
and TLF
from interface:
clear VLF
clear THF
and TLF
from interface:
clear PORF
from interface:
clear BSF
INT
(2)
 
 
(2) When bits TIE, UIE, AIE, EIE, TLIE, THIE, BSIE, PORIE (EEPROM C1h) and VLIE (EEPROM C1h) are 
disabled, pin INTÌ…Ì…Ì…Ì…Ì… remains high impedance. 
(6) Start up (POR) only possible via VDD pin.
```

## Page 77

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 77/154 Rev. 1.3 
4.8. PERIODIC COUNTDOWN TIMER INTERRUPT FUNCTION 
The Periodic Countdown Timer Interrupt function generates an interrupt event periodically at any period set from 
244.14 Î¼s to 4095 minutes. 
 
When starting the countdown timer for the first time, only the first period does not have a fixed duration. The amount 
of inaccuracy for the first timer period depends on the selected source clock (see FIRST PERIOD DURATION). 
When an interrupt event is generated, the INTÌ…Ì…Ì…Ì…Ì… pin goes to the low level and the TF flag is set to 1 to indicate that an 
event has occurred. The output on the INTÌ…Ì…Ì…Ì…Ì… pin is only effective if the TIE bit in the Control 2 register is set to 1. The 
low-level output signal on INTÌ…Ì…Ì…Ì…Ì… pin is automatically cleared after the Auto reset time t RTN1 or it is cancelled when TF  
flag is cleared to 0. 
? When TD = 00, tRTN1 = 122 µs 
? When TD = 01, 10 or 11, tRTN1 = 7.813 ms 
 
When bit TIE is set to 1, the internal countdown timer interrupt pulse (TI) can be used to enable the clock output on 
CLKOUT pin automatically. See PROGRAMMABLE CLOCK OUTPUT. 
 
 
4.8.1. PERIODIC COUNTDOWN TIMER DIAGRAM 
Diagram of the Periodic Countdown Timer Interrupt function: Example with interrupt on INTÌ…Ì…Ì…Ì…Ì… pin (TIE = 1). 
 
 
TIE 
TE
1. period
TF
period period period
1
3 6
4 5
8
Write operation
7
9
event 2
INT
tRTN1tRTN1tRTN1 tRTN1
period
 
1
 The Periodic Countdown Timer starts from the preset Timer Value in the registers 0Bh and 0Ch when 
  writing 1 to the TE bit. The countdown is based on the Timer Clock Frequency selected in the TD field. 
2
 When the countdown value reaches 000h, an interrupt event occurs. After the interrupt the counter is 
  automatically reloaded with the preset Timer Value, and starts again the countdown. 
3
 When a Periodic Countdown Timer Interrupt occurs, the TF flag is set to 1. 
4
 If bit TIE is 1 and a Periodic Countdown Timer Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin output pin goes LOW. 
5
 The INTÌ…Ì…Ì…Ì…Ì… output pin remains LOW during the Auto reset time t RTN1, and then it is automatically cleared to high 
  impedance. The TD field determines the Timer Clock Frequency and the Auto reset time t RTN1. 
  tRTN1 = 122 µs (TD = 00) or tRTN1 = 7.813 ms (TD = 01, 10, 11). 
6
 The TF flag retains 1 until it is cleared to 0 by software. 
7
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status change as soon as TF flag is cleared to 0. 
8
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status change as soon as TIE bit is cleared to 0. 
9
 When a 0 is written to the TE bit, the Periodic Countdown Timer function is stopped and the INTÌ…Ì…Ì…Ì…Ì… pin is 
  cleared after the Auto reset time tRTN1.
```

## Page 78

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 78/154 Rev. 1.3 
4.8.2. USE OF THE PERIODIC COUNTDOWN TIMER INTERRUPT 
The following registers, fields and bits are related to the Periodic Countdown Timer Interrupt and Interrupt Controlled 
Clock Output functions: 
 
? Timer Value 0 Register (0Bh) (see PERIODIC COUNTDOWN TIMER CONTROL REGISTERS) 
? Timer Value 1 Register (0Ch) (see PERIODIC COUNTDOWN TIMER CONTROL REGISTERS) 
? TF flag (see STATUS REGISTER, 0Dh - Status) 
? TE bit and TD field (see CONTROL REGISTERS, 10h - Control 1) 
? TIE bit (see CONTROL REGISTERS, 11h - Control 2) 
? INTDE and CTIE bits (see CLOCK INTERRUPT MASK REGISTER, 14h - Clock Interrupt Mask) 
 
See also INTERRUPT CONTROLLED CLOCK OUTPUT. 
 
Prior to entering any timer settings for the Periodic Countdown Timer Interrupt, it is recommended to write a 0 to the 
TIE and TE bits to prevent inadvertent interrupts on INTÌ…Ì…Ì…Ì…Ì… pin. The Timer Clock Frequency selection field TD is used 
to set the countdown period (source clock) for the Periodic Countdown Timer Interrupt function (four settings are 
possible). When STOP bit is set to 1 the interrupt function is stopped. When writing to the Seconds register or when 
ESYN bit is 1 in case of an External Event detection on EVI pin the length of the current countdown period is affected 
(see TIME SYNCHRONIZATION ). When the Periodic Countdown Timer Interrupt function is not used, the Timer 
Value 0 register (0Bh) can be used as RAM byte. 
 
Procedure to start the Periodic Countdown Timer Interrupt function and Interrupt Controlled Clock Output functions: 
 
1. Initialize bits TE, TIE and TF to 0. In that order, to prevent inadvertent interrupts on INTÌ…Ì…Ì…Ì…Ì… pin. 
2. Choose the Timer Clock Frequency and write the corresponding value in the TD field. 
3. Choose the Countdown Period based on the Timer Clock Frequency , and write the corresponding Timer 
Value to the registers Timer Value 0 (0Bh) and Timer Value 1 (0Ch). See following table. 
4. Set the TIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin or if you want to use the Interrupt 
Controlled Clock Output function. 
5. Set CTIE bit to 1 if you want to enable clock output when a timer interrupt occurs. 
6. Set INTDE bit to 1 if you want to enable interrupt Delay after CLKOUT On. 
7. Set the TE bit from 0 to 1 to start the Periodic Countdown Timer. The countdown starts at the rising edge of 
the SCL signal after Bit 0 of the Address 10h is transferred. See subsequent Figure that shows the start 
timing. 
 
Countdown Period in seconds: 
 
Countdown Period = Timer Value
Timer Clock Frequency 
 
Countdown Period: 
Timer Value 
(0Bh and 0Ch) 
Countdown Period 
TD = 00 (4096 Hz) TD = 01 (64 Hz) TD = 10 (1 Hz) TD = 11 (1/60 Hz) ) 
0 - - - - 
1 244.14 Î¼s 15.625 ms 1 s 1 min 
2 488.28 Î¼s 31.25 ms 2 s 2 min 
: : : : : 
41 10.010 ms 640.63 ms 41 s 41 min 
205 50.049 ms 3.203 s 205 s 205 min 
410 100.10 ms 6.406 s 410 s 410 min 
2048 500.00 ms 32.000 s 2048 s 2048 min 
: : : : : 
4095 (FFFh) 0.9998 s 63.984 s 4095 s 4095 min
```

## Page 79

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 79/154 Rev. 1.3 
General countdown timer behavior: 
 
Timer Value 1 
Timer Clock
TE
TF
INT
period period
The first period has an uncertainty.
See table "First period duration for Timer Value n"
Timer Value 0 xx 03
Internal
countdown
counter
xx 03 02 01 03 02 01 03 02 01 03
xx 00
xx 00
Write operation
 
 
In this example, it is assumed that the Timer Flag TF is cleared by software before the next countdown period 
expires. 
 
 
Start timing of the Periodic Countdown Timer: 
 
SCL
SDA
1. period
Address 10h
Internal Timer
USEL TE EERD TD1 TD0 ACK
Bit 4 Bit 3 Bit 2 Bit 1 Bit 0
event
Rising edge of the SCL signal 
INT
```

## Page 80

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 80/154 Rev. 1.3 
4.8.3. FIRST PERIOD DURATION 
When the TF flag is set, it indicates that an interrupt signal on INTÌ…Ì…Ì…Ì…Ì… is generated if this mode is enabled. See Section 
INTERRUPT OUTPUT for details on how the interrupt can be controlled. 
 
When starting the timer for the first time, the first period has an uncertainty. The uncertainty arises because of the  
activation instruction of the interface clock, which is not synchronous to the Timer Clock Frequency. Subsequent 
timer periods do not hav e such deviation. The amount of deviation for the first timer period depends on the chosen 
Timer Clock Frequency, see following Table. 
 
First period duration for Timer Value n (1): 
TD value Timer Clock Frequency First period duration Subsequent 
periods duration Minimum Period Maximum Period 
00 4096 Hz n × 244 µs + 61 µs (n + 1) × 244 µs + 61 µs n × 244 µs 
01 64 Hz n × 15.625 ms (n + 1) × 15.625 ms n × 15.625 ms 
10 1 Hz n × 1 s (n + 1) × 1 s n × 1 s 
11 1/60 Hz n × 60 s (n + 1) × 60 s n × 60 s 
(1) Timer Values n from 1 to 4095 are valid. When the Timer Value is set to 0, the countdown timer does not start. 
 
At the end of every countdown, the timer sets the Periodic Countdown Timer Flag (bit TF in Status Register). The TF 
flag can only be cleared by command. When enabled, a pulse is generated at the interrupt pin INTÌ…Ì…Ì…Ì…Ì…. 
 
When reading the Timer Value  (Timer Value 0 and Timer Value 1), the preset value is returned and not the current 
value.
```

## Page 81

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 81/154 Rev. 1.3 
4.9. PERIODIC TIME UPDATE INTERRUPT FUNCTION 
The Periodic Time Update Interrupt function generates an interrupt event periodically at the One-Second or the One-
Minute update time, according to the selected timer source with bit USEL.  
When an interrupt event is generated, the INTÌ…Ì…Ì…Ì…Ì… pin goes to the low level and the UF flag is set to 1 to indicate that an 
event has occurred. The output on INTÌ…Ì…Ì…Ì…Ì… pin is only effective if UIE bit in Control 2 register is set to 1. The low -level 
output signal on the INTÌ…Ì…Ì…Ì…Ì… pin is automatically cleared after the Auto reset time t RTN2 or it is cancelled when UF flag is 
cleared to 0 or when UIE is cleared to 0. 
? When USEL = 0 (Second update), tRTN2 = 500 ms 
? When USEL = 1 (Minute update), tRTN2 = 15.6 ms 
 
When bit UIE is set to 1, the internal update interrupt pulse (UI) can be used to enable the clock o utput on CLKOUT 
pin automatically. See PROGRAMMABLE CLOCK OUTPUT. 
 
 
4.9.1. PERIODIC TIME UPDATE DIAGRAM 
Diagram of the Periodic Time Update Interrupt function: 
 
 
UIE
UF
period period period
4 5
7
2 3
1
Write operation
6
event
INT
period
tRTN2tRTN2tRTN2 tRTN2
8
 
 
1
 A Periodic Time Update Interrupt event occurs when the internal clock value matches either the second or 
  the minute update time. The USEL bit determines whether it is the Second or the Minute period with the 
  corresponding Auto reset time tRTN2. tRTN2 = 500 ms (Second update) or tRTN2 = 15.6 ms (Minute update). 
2
 If UF flag was cleared beforehand and when a Periodic Time Update Interrupt occurs, the flag UF is set to 1. 
3
 The UF flag retains 1 until it is cleared to 0 by software. 
4
 If the UIE bit is 1 and a Periodic Time Update Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
5
The INTÌ…Ì…Ì…Ì…Ì… pin output remains LOW during the Auto reset time tRTN2, and then it is automatically cleared to high 
  impedance. 
6
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as UF flag is cleared to 0. 
7
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as UIE bit is cleared to 0. 
8
 When UIE bit is 0 and a Periodic Time Update Interrupt event occurs, the UF flag is not set and the INTÌ…Ì…Ì…Ì…Ì… pin 
  output does not go low.
```

## Page 82

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 82/154 Rev. 1.3 
4.9.2. USE OF THE PERIODIC TIME UPDATE INTERRUPT 
The following bits are related to the Periodic Time Update Interrupt and Interrupt Controlled Clock Output functions: 
 
? UF flag (see STATUS REGISTER, 0Dh - Status) 
? USEL bit (see CONTROL REGISTERS, 10h - Control 1) 
? UIE bit (see CONTROL REGISTERS, 11h - Control 2) 
? INTDE and CUIE bits (see CLOCK INTERRUPT MASK REGISTER, 14h - Clock Interrupt Mask) 
 
See also INTERRUPT CONTROLLED CLOCK OUTPUT. 
 
Prior to entering any other settings, it is recommended to write a 0 to the UIE bit to prevent inadvertent interrupts on 
INTÌ…Ì…Ì…Ì…Ì… pin. When STOP bit is set to 1 the interrupt function is stopped. When writing to the Seconds register or when 
ESYN bit is 1 in case of an External Event detection on EVI pin the length of the current update period is affected 
(see TIME SYNCHRONIZATION). 
 
Procedure to use the Periodic Time Update Interrupt and Interrupt Controlled Clock Output functions: 
 
1. Initialize bits UIE and UF to 0. 
2. Choose the timer source clock and write the corresponding value in the USEL bit. 
3. Set the UIE bit to 1 to enable the Periodic Time Update Interrupt function with hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin 
or if you want to use the Interrupt Controlled Clock Output function. 
4. Set CUIE bit to 1 if you want to enable clock output when a time update interrupt occurs. 
5. Set INTDE bit to 1 if you want to enable interrupt Delay after CLKOUT On. 
6. The first interrupt will occur after the next event, either second or minute change.
```

## Page 83

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 83/154 Rev. 1.3 
4.10. ALARM INTERRUPT FUNCTION 
The Alarm Interrupt function generates an interrupt for alarm settings such as date, hours and minutes settings. When 
an interrupt event is generated, the INTÌ…Ì…Ì…Ì…Ì… pin goes to the low level and the AF flag is set to 1 to indicate that an e vent 
has occurred. The output on the INTÌ…Ì…Ì…Ì…Ì… pin is only effective if the AIE bit in the Control 2 register is set to 1. 
 
When bit AIE is set to 1, the internal alarm interrupt signal (AI) can be used to enable the clock output on CLKOUT 
pin automatically. See PROGRAMMABLE CLOCK OUTPUT. 
 
 
4.10.1. ALARM DIAGRAM 
Diagram of the Alarm Interrupt function: 
 
 
AIE
AF
alarm
4
5
72 3
1
alarm
Write operation
6
event
INT
 
 
1
 A date, hours or minutes alarm interrupt event occurs when all selected Alarm registers (AE_x bits) 
  match to the respective counters. 
2
 When an Alarm Interrupt event occurs, the AF flag is set to 1. 
3
 The AF flag retains 1 until it is cleared to 0 by software. 
4
 If the AIE bit is 1 and an Alarm Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
5
 If the AIE value is changed from 1 to 0 while the INTÌ…Ì…Ì…Ì…Ì… pin output is LOW, the INTÌ…Ì…Ì…Ì…Ì… pin immediately 
  changes its status. While the AF flag is 1, the INTÌ…Ì…Ì…Ì…Ì… status can be controlled by the AIE bit. 
6
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as the AF flag is cleared from 1 to 0. 
7
 If the AIE bit value is 0 when an Alarm Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin status does not go LOW.
```

## Page 84

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 84/154 Rev. 1.3 
4.10.2. USE OF THE ALARM INTERRUPT 
The following registers and bits are related to the Alarm Interrupt and Interrupt Controlled Clock Output functions: 
 
? Minutes Register (02h) (see CLOCK REGISTERS) 
? Hours Register (03h) (see CLOCK REGISTERS) 
? Date Register (05h) (see CALENDAR REGISTERS) 
? Minutes Alarm Register and AE_M bit (08h) (see ALARM REGISTERS) 
? Hours Alarm Register and AE_H bit (09h) (see ALARM REGISTERS) 
? Date Alarm Register and AE_D bit (0Ah) (see ALARM REGISTERS) 
? AF flag (see STATUS REGISTER, 0Dh - Status) 
? AIE bit (see CONTROL REGISTERS, 11h - Control 2) 
? INTDE and CAIE bits (see CLOCK INTERRUPT MASK REGISTER, 14h - Clock Interrupt Mask) 
 
See also INTERRUPT CONTROLLED CLOCK OUTPUT. 
 
Prior to entering any timer settings for the Alarm Interrupt, it is recommended to write a 0 to the AIE bit to prevent 
inadvertent interrupts on INTÌ…Ì…Ì…Ì…Ì… pin. When STOP bit is set to 1 the interrupt function is stopped. When writing to the 
Seconds register or when ESYN bit is 1 in case of an External Even t detection on EVI pin the length of the  time to 
the next alarm interrupt is affected (see TIME SYNCHRONIZATION). When the Alarm Interrupt function is not used, 
one Byte (08h) of the Alarm registers can be used as RAM byte. In such case, be sure to write a 0 to the AIE bit (if 
the AIE bit value is 1 and the Alarm register is used as RAM register, INTÌ…Ì…Ì…Ì…Ì… may change to low level unintentionally). 
 
Procedure to use the Alarm Interrupt and Interrupt Controlled Clock Output functions: 
 
1. Initialize bits AIE and AF to 0. 
2. Write the desired alarm settings in registers 08h to 0A h. The three alarm enable bits, AE_M, AE_H and 
AE_D, are used to select the corresponding register that has to be taken into account for match or not. See 
the following table. 
3. Set CAIE bit to 1 if you want to enable clock output when an alarm occurs. 
4. Set INTDE bit to 1 if you want to enable interrupt Delay after CLKOUT On. 
5. Set the AIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin or if you want to use the Interrupt 
Controlled Clock Output function. 
 
Alarm Interrupt: 
Alarm enable bits Alarm event AE_D AE_H AE_M 
0 0 0 When minutes, hours and date match (once per month) (1) (2) - Default value 
0 0 1 When hours and date match (once per month) (1) (2) 
0 1 0 When minutes and date match (once per hour per month) (1) (2) 
0 1 1 When date matches (once per month) (1) (2) 
1 0 0 When minutes and hours match (once per day) (1) 
1 0 1 When hours match (once per day) (1) 
1 1 0 When minutes match (once per hour) (1) 
1 1 1 Every minute (3) 
(1) AE_x bits (where x is D, H and M) 
  AE_x = 0: Alarm is enabled 
  AE_x = 1: Alarm is disabled 
(2) When AE_D = 0 = enabled (default) and Date Alarm value = 00h = not valid (default) the Alarm function is deactivated (AF flag is never 
  set). 
(3) Note that if all AE_x = 1, there is no match, but an alarm event occurs every minute.
```

## Page 85

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 85/154 Rev. 1.3 
4.11. EXTERNAL EVENT INTERRUPT FUNCTION 
The External Event Interrupt is enabled by control bit EIE. The Time Stamp EVI function is always working (except 
in VBACKUP Power state). With the ET field the EVI input events can be configured either for edge detection,  or for 
edge & level detection with filtering, and with the EHL bit the active edge/level can be configured. 
In VBACKUP Power state, the External Event function is disabled. 
 
If enabled (EIE = 1 and EVF flag was cleared to 0 before ) and an External Event on EVI pin is detected , the clock 
and calendar registers  are captured and copied into the Time Stamp EVI registers, the INTÌ…Ì…Ì…Ì…Ì… is issued and the EVF 
flag is set to 1 to indicate that an external event has occurred. 
 
When bit EIE is set to 1, the internal signal (EI) of the external event interrupt can be used to enable the clock output 
on CLKOUT pin automatically. See PROGRAMMABLE CLOCK OUTPUT.
```

## Page 86

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 86/154 Rev. 1.3 
4.11.1. EXTERNAL EVENT DIAGRAM 
Diagram of  the External Event Interrupt function. Example with EHL = 1 for rising edge/high level detection and 
without time synchronization function (ESYN = 0): 
 
 
A+3
EIE
EVF
event
3 7
EVI
Set value to AClock & Calendar
registers
TS EVI Count register
A+1 A+2 A+3 A+4
6
00h if reseted 01h 02h 03h
event event
2
5
INT
EVR
A+5
TS EVI registers
(EVOW = 1)
00h if reseted A+1 All 00h
Write operation
TS EVI registers
(EVOW = 0, default)
00h if reseted A+1 All 00h
00h
A+2
8
9
1
4
 
1
 Initialize clock and calendar if a Time Stamp from the External Event Interrupt function is needed and select 
  EVOW (0 or 1). Set EIE bit to 1 if interrupt on INTÌ…Ì…Ì…Ì…Ì… pin is required. The EVF flag needs to be cleared to reset 
  the INTÌ…Ì…Ì…Ì…Ì… pin and to prepare the system for an event. The EVR bit does not need to be initialized. In this 
  example, EHL = 1 for rising edge/high level detection. Select edge detection (ET = 00) or edge & level 
  detection with filtering (ET ≠ 00). 
2
 An External Event on EVI pin is detected. Pay attention to the debounce time when using filtering (ET ≠ 00). 
  The value (A+1) is captured/copied into the TS EVI registers and the value in the TS EVI Count register is 
  incremented by one. The TS EVI Count register always counts events, regardless of the settings of the 
  override bit EVOW. 
3
 When an External Event Interrupt occurs, the EVF flag is set to 1. 
4
 If the EIE bit is 1 and an External Event Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
5
 The EVF flag retains 1 until it is cleared to 0 by software. 
6
 No interrupt occurs on INTÌ…Ì…Ì…Ì…Ì… pin because the EVF flag was not set back to 0. But, new value (A+2) is captured 
  in the TS EVI registers if the Time Stamp overwrite bit EVOW is set to 1. 
7
 If the INTÌ…Ì…Ì…Ì…Ì… pin is low, its status changes as soon as the EVF flag is cleared to 0, even if EVI input is high level.  
8
 While the EVF flag is 1, the INTÌ…Ì…Ì…Ì…Ì… status can be controlled by the EIE bit. 
9
 When writing 1 to EVR bit, all eight time stamp registers (TS EVI Count to TS EVI Year) are reset to 00h. Bit 
  EVR can be left at 1. Writing or overwriting a 1 causes reset.
```

## Page 87

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 87/154 Rev. 1.3 
4.11.2. USE OF THE EXTERNAL EVENT INTERRUPT 
The following registers and bits are related to the External Event Interrupt, EVI Time Stamp and Interrupt Controlled 
Clock Output functions: 
 
? 100th Seconds Register (00h) (see CLOCK REGISTERS) 
? Seconds Register (01h) (see CLOCK REGISTERS) 
? Minutes Register (02h) (see CLOCK REGISTERS) 
? Hours Register (03h) (see CLOCK REGISTERS) 
? Date Register (05h) (see CALENDAR REGISTERS) 
? Month Register (06h) (see CALENDAR REGISTERS) 
? Year Register (07h ) (see CALENDAR REGISTERS) 
? TS EVI Count Register (26h) (see TIME STAMP EVI REGISTERS) 
? TS EVI 100th Seconds Register (27h) (see TIME STAMP EVI REGISTERS) 
? TS EVI Seconds (28h) (see TIME STAMP EVI REGISTERS) 
? TS EVI Minutes (29h) (see TIME STAMP EVI REGISTERS) 
? TS EVI Hours (2Ah) (see TIME STAMP EVI REGISTERS) 
? TS EVI Date (2Bh) (see TIME STAMP EVI REGISTERS) 
? TS EVI Month (2Ch) (see TIME STAMP EVI REGISTERS) 
? TS EVI Year (2Dh) (see TIME STAMP EVI REGISTERS) 
? EVF flag (see STATUS REGISTER, 0Dh - Status) 
? EIE bit (see CONTROL REGISTERS, 11h - Control 2) 
? EVR and EVOW bits (see TIME STAMP CONTROL REGISTER, 13h - Time Stamp Control) 
? INTDE and CEIE bits (see CLOCK INTERRUPT MASK REGISTER, 14h - Clock Interrupt Mask) 
? EHL bit, ET field and ESYN bit (see EVI CONTROL REGISTER, 15h - EVI Control) 
 
See also INTERRUPT CONTROLLED CLOCK OUTPUT. 
 
Prior to entering any other settings, it is recommended to write a 0 to the EIE bit to prevent inadvertent interrupts on 
INTÌ…Ì…Ì…Ì…Ì… pin. When STOP bit is set to 1 the interrupt function is still working , but because the 1 Hz tick  is stopped and 
100th Seconds register is reset to 0 0, it cannot provide useful data . When writing to the Seconds register or when 
ESYN bit is 1 in case of an Exter nal Event detection on EVI pin the length of the  time to the next alarm interrupt is 
affected (see TIME SYNCHRONIZATION). 
 
Procedure to use the External Event Interrupt, EVI Time Stamp and Interrupt Controlled Clock Output functions: 
 
1. Initialize bit EIE to 0. 
2. Clear flag EVF to 0. 
3. Set EHL bit to 0 or 1 to choose falling edge/low level or rising edge/high level detection on pin EVI. 
4. Select EDGE DETECTION (ET = 00) or LEVEL DETECTION WITH FILTERING (ET ≠ 00). 
5. Set EVOW bit to 1 if the last occurred event has to be recorded and TS EVI registers are overwritten. 
Hint: The TS EVI Count register always counts events, regardless of the settings of the override bit EVOW.  
6. Write 1 to EVR bit, to reset all Time Stamp EVI registers to 00h. The bit EVR does not need to be reset. 
7. Set CEIE bit to 1 if you want to enable clock output when external event occurs. See also CLOCK 
OUTPUT SCHEME. 
8. Set INTDE bit to 1 if you want to enable interrupt Delay after CLKOUT On. 
9. Set EIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin or if you want to use the Interrupt 
Controlled Clock Output function.
```

## Page 88

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 88/154 Rev. 1.3 
4.11.3. EDGE DETECTION (ET = 00) 
Example with rising edge detection and interrupt output: 
 
 
EVI
INT
EVF 21
Write operation
min. 30.5 µs
tDELAY
Between 30.5 µs and
61 µs (random)
 
 
  Settings: EHL = 1, ET = 00, EIE = 1. 
1
 When a rising edge on EVI pin is detected, the EVF flag is set to 1 and the INTÌ…Ì…Ì…Ì…Ì… output pin goes LOW. 
2
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as the EVF flag is cleared to 0, even if EVI input is high 
  level. 
 
 
 
4.11.4. LEVEL DETECTION WITH FILTERING (ET ≠ 00) 
Example with high level detection (rising edge & high level) and interrupt output: 
 
EVI
sampling
tSP < tDELAY  < 2 tSP
1 1 1 2
tSPtSPtSP
EVF
Write operation
INT
3
tSP
4
 
 
  Settings: EHL = 1, ET ≠ 00, EIE = 1. 
  Available sampling periods for the digital debounce filtering: 
                 ET = 01, tSP =  3.9 ms 
                 ET = 10, tSP =  15.6 ms 
                 ET = 11, tSP = 125.0 ms 
1
 From the previous sampling pulse to this sampling pulse a positive edge was detected. 
2
 If a positive edge was detected in the previous period and a stable high level on EVI pin was detected in the 
  following sampling period, the EVF flag is set to 1 and the INTÌ…Ì…Ì…Ì…Ì… output pin goes LOW. The delay time tDELAY is 
  between tSP and 2 tSP. 
3
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as the EVF flag is cleared to 0. 
4
 In subsequent stable high level periods, the interrupt is not triggered.
```

## Page 89

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 89/154 Rev. 1.3 
4.12. TEMPERATURE LOW INTERRUPT FUNCTION 
The Temperature Low Interrupt and the Time Stamp TLow function are enabled by the control bits TLE and TLIE. 
The Temperature Low Threshold value TLT which is compared with the TEMP [11:4] value can be defined in register 
16h. 
 
If enabled (TLE = 1 and TLIE = 1 and TLF flag was cleared to 0 before) and TEMP [11:4] < TLT is detected (automatic 
temperature measurement is carried out once per second), the clock and calendar registers are captured and copied 
into the Time Stamp TLow registers, the INTÌ…Ì…Ì…Ì…Ì… is issued and the TLF flag is set to 1 to indicate that a temperature low 
event has occurred. 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).  
 
When bit TLIE is set to 1, the internal signal (TLI) of the temperature low event interrupt can be used to enable the 
clock output on CLKOUT pin automatically. See PROGRAMMABLE CLOCK OUTPUT.
```

## Page 90

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 90/154 Rev. 1.3 
4.12.1. TEMPERATURE LOW DIAGRAM 
Diagram of the Temperature Low Interrupt function. Example with Temperature Low detection enabled (TLE = 1): 
 
 
A+4
TLIE
TLF
event
3 7
TEMP [11:4] < TLT
(1 Hz sampling)
Set value to AClock & Calendar
registers
TS TLow Count
register
A+1 A+2 A+3 A+4
6
00h if reseted 01h 02h 03h
event event
2
5
INT
TLR
A+5
TS TLow registers
(TLOW = 1)
00h if reseted A+1 All 00h
Write operation
TS TLow registers
(TLOW = 0, default)
00h if reseted A+1 All 00h
00h
A+2
8
9
1
4
 
 
1
 Initialize clock and calendar if a Time Stamp from the TLow Interrupt function is needed and select 
  TLOW (0 or 1). Enter the desired Temperature Low Threshold value TLT. Set TLIE bit to 1 if interrupt on INTÌ…Ì…Ì…Ì…Ì… 
  pin is required. The TLF flag needs to be cleared to reset the INTÌ…Ì…Ì…Ì…Ì… pin and to prepare the system for an event. 
2
 A TLow event is detected. The value (A+1) is captured/copied into the TS TLow registers and the value in 
  the TS TLow Count register is incremented by one. The TS TLow Count register always counts events, 
  regardless of the settings of the override bit TLOW. 
3
 When a TLow Interrupt occurs, the TLF flag is set to 1. 
4
 If the TLIE bit is 1 and a TLow Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
5
 The TLF flag retains 1 until it is cleared to 0 by software. 
6
 No interrupt occurs on INTÌ…Ì…Ì…Ì…Ì… pin because the TLF flag was not set back to 0. But, new value (A+2) is captured 
  in the TS TLow registers if the Time Stamp overwrite bit TLOW is set to 1. 
7
 If the INTÌ…Ì…Ì…Ì…Ì… pin is low, its status changes as soon as the TLF flag is cleared to 0, even if TEMP [11:4] < TLT. 
8
 While the TLF flag is 1, the INTÌ…Ì…Ì…Ì…Ì… status can be controlled by the TLIE bit. 
9
 When writing 1 to TLR bit, all seven time stamp registers (TS TLow Count to TS TLow Year) are reset to 00h. 
  The TLF flag is also cleared to 0 when writing 1 to the TLR bit and INTÌ…Ì…Ì…Ì…Ì… pin goes high impedance. Bit TLR 
  always returns 0 when read. 
 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 91

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 91/154 Rev. 1.3 
4.12.2. USE OF THE TEMPERATURE LOW INTERRUPT 
The following registers and bits are related to the Temperature Low  Interrupt, TLow Time Stamp and Interrupt 
Controlled Clock Output functions: 
 
? Seconds Register (01h) (see CLOCK REGISTERS) 
? Minutes Register (02h) (see CLOCK REGISTERS) 
? Hours Register (03h) (see CLOCK REGISTERS) 
? Date Register (05h) (see CALENDAR REGISTERS) 
? Month Register (06h) (see CALENDAR REGISTERS) 
? Year Register (07h ) (see CALENDAR REGISTERS) 
? TLow Threshold Register (16h) (see TEMPERATURE THRESHOLDS REGISTERS) 
? TS TLow Count Register (18h) (see TIME STAMP TLOW REGISTERS) 
? TS TLow Seconds (19h) (see TIME STAMP TLOW REGISTERS) 
? TS TLow Minutes (1Ah) (see TIME STAMP TLOW REGISTERS) 
? TS TLow Hours (1Bh) (see TIME STAMP TLOW REGISTERS) 
? TS TLow Date (1Ch) (see TIME STAMP TLOW REGISTERS) 
? TS TLow Month (1Dh) (see TIME STAMP TLOW REGISTERS) 
? TS TLow Year (1Eh) (see TIME STAMP TLOW REGISTERS) 
? TLF flag (see STATUS REGISTER, 0Dh - Status) 
? TLE and TLIE bits (see CONTROL REGISTERS, 12h - Control 3) 
? TLR and TLOW bits (see TIME STAMP CONTROL REGISTER, 13h - Time Stamp Control) 
? INTDE and CTLIE bits (see CLOCK INTERRUPT MASK REGISTER, 14h - Clock Interrupt Mask) 
 
See also INTERRUPT CONTROLLED CLOCK OUTPUT. 
 
Prior to entering any timer settings for the Temperature Low Interrupt, it is recommended to write a 0 to the TLE and 
TLIE bits to prevent inadvertent interrupts on INTÌ…Ì…Ì…Ì…Ì… pin. When the STOP bit value is 1, the Temperature Low Interrupt 
function cannot provide new data because the 1 Hz tick is stopped  and because temperature measurement, 
temperature compensation and temperature comparison with the TLT value is stopped. When writing to the Seconds 
register or when the ESYN bit is 1 in case of an External Event detection on EVI pin the time is synchronized and the 
100th Seconds register is reset to 00 (see TIME SYNCHRONIZATION). 
When the Temperature Low Interrupt function is not used, the TLow Threshold register (16h) can be used as RAM 
byte. In such case, be sure to write a 0 to the TLE and TLIE bits (if the TLE and TLIE  bit values are 1 and the TLow 
Threshold register is used as RAM register, INTÌ…Ì…Ì…Ì…Ì… may change to low level unintentionally). 
 
Procedure to use the Temperature Low Interrupt, TLow Time Stamp and Interrupt Controlled Clock Output functions: 
 
1. Initialize bits TLE and TLIE to 0. 
2. Clear flag TLF to 0. 
3. Enter the desired Temperature Low Threshold value TLT. 
4. Set TLOW bit to 1 if the last occurred event has to be recorded and TS TLow registers are overwritten. 
Hint: The TS TLow Count register always counts events, regardless of the settings of the override bit 
TLOW. 
5. Write 1 to TLR bit, to reset all Time Stamp TLow registers to 00h. With this reset, the TLF flag is also 
automatically reset to 0. The TLR bit always returns 0 when read. 
6. Set CTLIE bit to 1 if you want to enable clock output when temperature low interrupt occurs. See also 
CLOCK OUTPUT SCHEME. 
7. Set INTDE bit to 1 if you want to enable interrupt Delay after CLKOUT On. 
8. Set TLIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin or if you want to use the Interrupt 
Controlled Clock Output function. 
9. Set the TLE bit from 0 to 1 to start the Temperature Low detection. 
 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 92

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 92/154 Rev. 1.3 
4.13. TEMPERATURE HIGH INTERRUPT FUNCTION 
The Temperature High Interrupt and the Time Stamp THigh function are enabled by the control bits THE and THIE. 
The Temperature High Threshold value THT which is compared with the TEMP [11:4] value can be defined in register 
17h. 
 
If enabled (THE = 1 and THIE =  1 and THF flag was cleared to 0 before) and TEMP [11:4] > THT is detected 
(automatic temperature measurement is carried out once per second), the clock and calendar registers are captured 
and copied into the Time Stamp THigh registers, the INTÌ…Ì…Ì…Ì…Ì… is issued an d the THF flag is set to 1 to indicate that a 
temperature high event has occurred. 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).  
 
When bit THIE is set to 1, the internal signal (THI) of the temperature high event interrupt can be used to enable the 
clock output on CLKOUT pin automatically. See PROGRAMMABLE CLOCK OUTPUT.
```

## Page 93

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 93/154 Rev. 1.3 
4.13.1. TEMPERATURE HIGH DIAGRAM 
Diagram of the Temperature High Interrupt function. Example with Temperature High detection enabled (THE = 1):  
 
 
A+4
THIE
THF
event
3 7
TEMP [11:4] > THT
(1 Hz sampling)
Set value to AClock & Calendar
registers
TS THigh Count
register
A+1 A+2 A+3 A+4
6
00h if reseted 01h 02h 03h
event event
2
5
INT
THR
A+5
TS THigh registers
(THOW = 1)
00h if reseted A+1 All 00h
Write operation
TS THigh registers
(THOW = 0, default)
00h if reseted A+1 All 00h
00h
A+2
8
9
1
4
 
 
1
 Initialize clock and calendar if a Time Stamp from the THigh Interrupt function is needed and select 
  THOW (0 or 1). Enter the desired Temperature High Threshold value THT. Set THIE bit to 1 if interrupt on 
  INTÌ…Ì…Ì…Ì…Ì… pin is required. The THF flag needs to be cleared to reset the INTÌ…Ì…Ì…Ì…Ì… pin and to prepare the system for an 
  event. 
2
 A THigh event is detected. The value (A+1) is captured/copied into the TS THigh registers and the value in 
  the TS THigh Count register is incremented by one. The TS THigh Count register always counts events, 
  regardless of the settings of the override bit THOW. 
3
 When a THigh Interrupt occurs, the THF flag is set to 1. 
4
 If the THIE bit is 1 and a THigh Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
5
 The THF flag retains 1 until it is cleared to 0 by software. 
6
 No interrupt occurs on INTÌ…Ì…Ì…Ì…Ì… pin because the THF flag was not set back to 0. But, new value (A+2) is captured 
  in the TS THigh registers if the Time Stamp overwrite bit THOW is set to 1. 
7
 If the INTÌ…Ì…Ì…Ì…Ì… pin is low, its status changes as soon as the THF flag is cleared to 0, even if TEMP [11:4] > THT. 
8
 While the THF flag is 1, the INTÌ…Ì…Ì…Ì…Ì… status can be controlled by the THIE bit. 
9
 When writing 1 to THR bit, all seven time stamp registers (TS THigh Count to TS THigh Year) are reset to 
  00h. The THF flag is also cleared to 0 when writing 1 to the THR bit and INTÌ…Ì…Ì…Ì…Ì… pin goes high impedance. Bit 
  THR always returns 0 when read. 
 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 94

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 94/154 Rev. 1.3 
4.13.2. USE OF THE TEMPERATURE HIGH INTERRUPT 
The following registers and bits are related to the Temperature LHigh Interrupt, THigh Time Stamp and Interrupt 
Controlled Clock Output functions: 
 
? Seconds Register (01h) (see CLOCK REGISTERS) 
? Minutes Register (02h) (see CLOCK REGISTERS) 
? Hours Register (03h) (see CLOCK REGISTERS) 
? Date Register (05h) (see CALENDAR REGISTERS) 
? Month Register (06h) (see CALENDAR REGISTERS) 
? Year Register (07h ) (see CALENDAR REGISTERS) 
? THigh Threshold Register (17h) (see TEMPERATURE THRESHOLDS REGISTERS) 
? TS THigh Count Register (1Fh) (see TIME STAMP THIGH REGISTERS) 
? TS THigh Seconds (20h) (see TIME STAMP THIGH REGISTERS) 
? TS THigh Minutes (21h) (see TIME STAMP THIGH REGISTERS) 
? TS THigh Hours (22h) (see TIME STAMP THIGH REGISTERS) 
? TS THigh Date (23h) (see TIME STAMP THIGH REGISTERS) 
? TS THigh Month (24h) (see TIME STAMP THIGH REGISTERS) 
? TS THigh Year (25h) (see TIME STAMP THIGH REGISTERS) 
? THF flag (see STATUS REGISTER, 0Dh - Status) 
? THE and THIE bits (see CONTROL REGISTERS, 12h - Control 3) 
? THR and THOW bits (see TIME STAMP CONTROL REGISTER, 13h - Time Stamp Control) 
? INTDE and CTHIE bits (see CLOCK INTERRUPT MASK REGISTER, 14h - Clock Interrupt Mask) 
 
See also INTERRUPT CONTROLLED CLOCK OUTPUT. 
 
Prior to entering any timer settings for the Temperature High Interrupt, it is recommended to write a 0 to the THE and 
THIE bits to prevent inadvertent interrupts on INTÌ…Ì…Ì…Ì…Ì… pin. When the STOP bit value is 1, the Temperature High Interrupt 
function cannot provide new data because the 1 Hz tick is stopped and because temperature measurement, 
temperature compensation and temperature comparison with the THT value is stopped. When writing to the Seconds 
register or when the ESYN bit is 1 in case of an External Event detection on EVI pin the time is synchronized and the 
100th Seconds register is reset to 00 (see TIME SYNCHRONIZATION). 
When the Temperature High Interrupt function is not used, the THigh Threshold register (17h) can be used as RAM 
byte. In such case, be sure to write a 0 to the THE and THIE bits (if the THE and THIE bit values are 1 and the THigh 
Threshold register is used as RAM register, INTÌ…Ì…Ì…Ì…Ì… may change to low level unintentionally). 
 
Procedure to use the Temperature High Interrupt, THigh Time Stamp and Interrupt Controlled Clock Output functions: 
 
1. Initialize bits THE and THIE to 0. 
2. Clear flag THF to 0. 
3. Enter the desired Temperature High Threshold value THT. 
4. Set THOW bit to 1 if the last occurred event has to be recorded and TS THigh registers are overwritten. 
Hint: The TS THigh Count register always counts events, regardless of the settings of the override bit 
THOW. 
5. Write 1 to THR bit, to reset all Time Stamp THigh registers to 00h. With this reset, the THF flag is also 
automatically reset to 0. The THR bit always returns 0 when read. 
6. Set CTHIE bit to 1 if you want to enable clock output when temperature high interrupt occurs. See also 
CLOCK OUTPUT SCHEME. 
7. Set INTDE bit to 1 if you want to enable interrupt Delay after CLKOUT On. 
8. Set THIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin or if you want to use the Interrupt 
Controlled Clock Output function. 
9. Set the THE bit from 0 to 1 to start the Temperature High detection. 
 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 95

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 95/154 Rev. 1.3 
4.14. AUTOMATIC BACKUP SWITCHOVER INTERRUPT FUNCTION 
The Automatic Backup Switchover Interrupt function generates an interrupt event when the BS M field (EEPROM 
C0h) is set to 01 (DSM) or 10 (LSM) and a switchover from VDD Power state to VBACKUP Power state occurs.  
 
If enabled (BSIE = 1 and BSF flag was cleared to 0 before) and a Backup Switchover is detected, the INTÌ…Ì…Ì…Ì…Ì… is issued 
and the BSF flag is set to 1 to indicate that a Backup Switchover has occurred. 
 
A debounce logic provides a debounce time tDEB which will filter VDD oscillation when switchover function will switch 
back from VBACKUP to VDD. I2C access is again possible in VDD Power state (and if VDD ≥ 1.4 V) after the debounce 
time tDEB. 
? tDEB MAX = 1 ms, when internal voltage was always above V LOW (typical 1.2 V). VLF = 0. 
? tDEB MAX = 1000 ms, when internal voltage was between V LOW (typical 1.2 V) and V POR (maximum 1.05 V). 
VLF = 1. See also BACKUP AND RECOVERY AC ELECTRICAL CHARACTERISTICS. 
 
 
4.14.1. AUTOMATIC BACKUP SWITCHOVER DIAGRAM 
Diagram of the Automatic Backup Switchover Interrupt function: 
 
 
BSIE
BSF
event
3 7
VBACKUP Power 6
event event
2
5
INT
81
Write operation (I2C access only in VDD Power state)
4
event
9
 
 
1
 Set BSIE bit to 1 if interrupt on INTÌ…Ì…Ì…Ì…Ì… pin is required. The BSF flag needs to be cleared to reset the INTÌ…Ì…Ì…Ì…Ì… pin and 
  to prepare the system for an event. To enable switchover function the BSM field (EEPROM C0h) is set to 
  01 (DSM) or 10 (LSM). 
2
 A backup switchover from VDD Power state to VBACKUP Power state occurs. 
3
 When an Automatic Backup Switchover event occurs, the BSF flag is set to 1. 
4
 If the BSIE bit is 1 and a Switchover Interrupt occurs, the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
5
 The BSF flag retains 1 until it is cleared to 0 by software. 
6
 No interrupt occurs on INTÌ…Ì…Ì…Ì…Ì… pin because the BSF flag was not set back to 0. 
7
 When the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as the BSF flag is cleared to 0. 
8
 While the BSF flag is 1, the INTÌ…Ì…Ì…Ì…Ì… status can be controlled by the BSIE bit. 
9
 If the BSIE bit value is 0 when a Switchover event occurs, the INTÌ…Ì…Ì…Ì…Ì… pin status does not go LOW.
```

## Page 96

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 96/154 Rev. 1.3 
4.14.2. USE OF THE AUTOMATIC BACKUP SWITCHOVER INTERRUPT 
The following field and bits are related to the Automatic Backup Switchover Interrupt function: 
 
? BSF flag (see TEMPERATURE REGISTERS, 0Eh - Temperature LSBs) 
? BSIE bit (see CONTROL REGISTERS, 12h - Control 3) 
? BSM field (see EEPROM PMU REGISTER, C0h - EEPROM PMU) 
 
See also EEPROM READ/WRITE. 
 
Prior to entering any other settings, it is recommended to write a 0 to the  BSIE bit to prevent inadvertent interrupts 
on INTÌ…Ì…Ì…Ì…Ì… pin. 
 
Procedure to use the Automatic Backup Switchover Interrupt function: 
 
1. Initialize bit BSIE to 0. 
2. Clear flag BSF to 0. 
3. Choose the Backup Switchover Mode (DSM or LSM) and write the corresponding value in the BSM field. 
4. Set the BSIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin.
```

## Page 97

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 97/154 Rev. 1.3 
4.15. POWER ON RESET INTERRUPT FUNCTION 
The Power On Reset Interrupt function is enabled by the PORIE control bit (EEPROM C1h). The PORIE bit has to 
be set beforehand in the EEPROM, not only in the RAM (see EEPROM READ/WRITE). 
When a VDD startup from below V POR (TYP 0.95 V)  in VDD Power state is detected,  the PORF flag is set to 1 to 
indicate that a Power On Reset has occurred and when the PORIE bit is 1 the INTÌ…Ì…Ì…Ì…Ì… pin goes to low level. The data in 
the device are no longer valid and all registers must be initialized.  The value 1 is retained until a 0 is written by the 
user. See also POWER ON AC ELECTRICAL CHARACTERISTICS. 
 
 
4.15.1. POWER ON RESET DIAGRAM 
Diagram of the Power On Reset Interrupt function: Example with Power On Reset Interrupt and CLKOUT enabled. 
 
 
tDELAY
tPOR
tDELAY
tPOR
Hi-Z Hi-Z
PORIE
PORF
VDD
7
3
5
8
6
INT
State POR Operating POR Operating
I2C access disabled enabled disabled
Write operation
enabled
CLKOUT
VPOR
tSTART
tPREFR
VPOR
tSTART
tPREFR
1
2
4
2
4
 
 
1
 CLKOUT is set to LOW, after power on reset tPOR  = typical 6 ms where CLKOUT is high-impedance (Hi-Z). 
2
 Flag PORF is set because VDD was below VPOR (Power On Reset event detected). 
3
 If the PORIE bit (EEPROM C1h) was set to 1 beforehand (in EEPROM), the PORIE bit in the RAM mirror is 
  set to 1 after the typical start-up time tSTART = 0.1 s including the first refreshment time tPREFR = ~66 ms and 
  the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
4
 CLKOUT is enabled after a typical delay time tDELAY, which depends on the selected frequency (synchronized 
  enable). See POWER ON AC ELECTRICAL CHARACTERISTICS. 
5
 The PORF flag retains 1 until it is cleared to 0 by software. 
6
 While the PORF flag is 1, the INTÌ…Ì…Ì…Ì…Ì…  status can be controlled by the PORIE bit. 
7
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as the PORF flag is cleared to 0. Note: When VDD voltage 
  falls below VPOR, PORF is automatically cleared to 0. 
8
 Like
3  
  Or else, if the PORIE bit (EEPROM 21h) was set to 0 beforehand (in EEPROM), the PORIE bit in the RAM is 
  set to 0 after the typical start-up time tSTART = 0.1 s and the INTÌ…Ì…Ì…Ì…Ì… pin output remains HIGH.
```

## Page 98

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 98/154 Rev. 1.3 
4.15.2. USE OF THE POWER ON RESET INTERRUPT 
The following registers and bits are related to the Power On Reset Interrupt function: 
 
? PORF flag (see STATUS REGISTER, 0Dh - Status) 
? PORIE bit (see EEPROM OFFSET REGISTER, C1h - EEPROM Offset) 
 
See also EEPROM READ/WRITE. 
 
The PORIE bit has to be set beforehand in the EEPROM, not in the RAM. 
 
Procedure to use the Power On Reset Interrupt function: 
 
1. In the EEPROM, set the PORIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin at the next Power 
On Reset event. Procedure according to EEPROM READ/WRITE. 
2. The first interrupt will occur after the next POR event.
```

## Page 99

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 99/154 Rev. 1.3 
4.16. VOLTAGE LOW INTERRUPT FUNCTION 
The Voltage Low Interrupt function generates an interrupt event when  a voltage drop of the internal power supply 
voltage below VLOW (typical 1.2 V) is detected. The sampling frequency is 1 Hz. If the internal voltage (VDD or VBACKUP) 
is below VLOW, the temperature compensation is stopped, CLKOUT is LOW and the I 2C interface is disabled (VDD < 
1.4 V). The interrupt enable bit VLIE is located in the EEPROM Register C1h.  
If enabled (VLIE = 1 and VLF flag was cleared to 0 before) and a voltage drop below V LOW is detected, the INTÌ…Ì…Ì…Ì…Ì… is 
issued and the VLF flag is set to 1 to indicate that a Voltage Low event has occurred. The VLF value of 1 indicates 
that the data in the device  may no longer be valid and all registers should be reinitialized. The value 1 is retained 
until a 0 is written by the user  (no automatic cancellation) . If the internal voltage is below V LOW, the temperature 
compensation is stopped, CLKOUT is LOW and the I2C interface is disabled (VDD < 1.4 V). 
 
 
4.16.1. VOLTAGE LOW DIAGRAM 
Diagram of the Voltage Low Interrupt function: Example with CLKOUT enabled. 
 
VPOR
Hi-Z
VLIE
VLF 8
1
3
INT
I2C access disabled
State POR Operating
disabled
Write operation
enabled
VDD
VLOW (typical 1.2 V)
Sampling
(1 Hz)
CLKOUT
tSTART
4
2 5
6
enabled
  1.1 V
< 1 s < 1 s
7
 
 
1
 If the VLIE bit (EEPROM C1h) was set to 1 beforehand (in EEPROM), the VLIE bit in the RAM mirror is set 
  to 1 after the start-up time tSTART = 0.1 s including the first refreshment time tPREFR = ~66 ms. 
2
 VDD voltage drops below VLOW (typical 1.2 V) to a voltage greater than the minimum timekeeping voltage 
  (typical 1.1 V). Temperature compensation is stopped and I2C access (VDD < 1.4 V) is disabled. 
3
 After a delay of < 1 second, VDD ≤ VLOW is detected, VLF flag is set and if VLIE bit is 1 the INTÌ…Ì…Ì…Ì…Ì… pin output 
  goes LOW. CLKOUT also goes LOW. 
4
 VLF flag cannot be read or cleared to 0 while the I2C access is disabled. 
5
 VDD voltage rises again above VLOW (typical 1.2 V). Temperature compensation and I2C access (VDD ≥ 1.4 V) 
  is enabled. The VLF flag retains 1 until it is cleared to 0 by software. 
6
 After a delay of < 1 second, VDD > VLOW. CLKOUT is enabled. 
7
 While the VLF flag is 1, the INTÌ…Ì…Ì…Ì…Ì…  status can be controlled by the VLIE bit. 
8
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as the VLF flag is cleared to 0.
```

## Page 100

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 100/154 Rev. 1.3 
4.16.2. USE OF THE VOLTAGE LOW INTERRUPT 
The following bits are related to the Voltage Low Interrupt function: 
 
? VLF flag (see STATUS REGISTER, 0Dh - Status) 
? VLIE bit (see EEPROM OFFSET REGISTER, C1h - EEPROM Offset) 
 
See also EEPROM READ/WRITE. 
 
Prior to entering any other settings, it is recommended to write a 0 to the VLIE bit to prevent inadvertent interrupts 
on INTÌ…Ì…Ì…Ì…Ì… pin. 
 
Procedure to use the Voltage Low Interrupt function: 
 
1. Initialize bit VLIE to 0. 
2. Clear flag VLF to 0. 
3. Set the VLIE bit to 1 if you want to get a hardware interrupt on INTÌ…Ì…Ì…Ì…Ì… pin. 
 
 
Application note: Since the Voltage Low Interrupt function can only detect a slow voltage sink rate (e.g. < 0.1 V/s), it 
is used as a low battery or an end-of-life (EOL) indicator. The Voltage Low signal can be output via the INTÌ…Ì…Ì…Ì…Ì… pin. 
? Note that the POR generated at startup has the VLF flag automatically cleared to 0 . 
Should the power-up time erroneously exceed to long, a low voltage can be detected between VPOR and VLOW 
(VLF flag is set). 
? Note that the sampling frequency of the internal voltage (VDD or VBACKUP) is 1 Hz. 
? Note that the Voltage Low Interrupt function cannot be used for a fast voltage drop detection.  
 
Example of rapid voltage changes (in the VDD Power state to show CLKOUT behavior) : 
? If the voltage drops rapidly from VDD > VLOW to VDD ≤ VPOR, sampling is missed and no interrupt is generated 
(VLF flag not set). CLKOUT turns off immediately. 
? When VDD rises again rapidly from VDD ≤ VPOR to VDD > VLOW, sampling is missed and no interrupt is 
generated (VLF flag not set). But as an exception, CLKOUT turns on immediately and not with the usual 
latency due to the 1 Hz sampling.
```

## Page 101

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 101/154 Rev. 1.3 
4.17. TIME STAMP EVI FUNCTION 
The Time Stamp EVI function is always working (except in VBACKUP Power state). 
When an External Event on EVI pin is detected , the clock and calendar registers are captured and copied into the 
Time Stamp EVI registers . When the EVOW bit is set to 0 and if the Time Stamp EVI registers were previously 
initialized to zero by writing 1 to the EVR  bit, only one (the first) event is recorded. When the EVOW bit is set to 1, 
the last occurred event is recorded and TS EVI registers  (TS EVI 100 th Seconds to TS EVI Year)  are overwritten. 
The TS EVI Count register always counts events, regardless of the settings of the override bit EVOW. 
When writing 1 to EVR bit, all eight time stamp registers (TS EVI Count to TS EVI Year) are reset to 00h. Bit EVR 
can be left at 1 . Writing or overwriting a 1 causes reset.  Hint: When EVI pin = HIGH, all Time Stamp EVI registers 
are also reset to 00h at POR. Before using the Time Stamp EVI function, it is recommended to write 1 to EVR bit.  
When STOP bit is set to 1 the interrupt function is still working, but because the 1 Hz tick is stopped and 100 th 
Seconds register is reset to 00, it cannot provide useful data. When writing to the Seconds register or when the ESYN 
bit is 1 in case of an External Event detection on EVI pin the time is synchronized and the 100 th Seconds register is 
reset to 00  (the ESYN bit function does first the Time Stamp EVI, then clears 100 th Seconds register) (see TIME 
SYNCHRONIZATION). 
 
Procedure for using the Time Stamp EVI function: 
1. Initialize bit EIE to 0. 
2. Select EVOW (0 or 1) and clear EVF flag. 
3. Write 1 to EVR bit, to reset all Time Stamp EVI registers to 00h. EVR may remain set. No further reset 
occurs. 
Hint: EVF flag is not reset by the EVR bit function. 
4. Initialize the EXTERNAL EVENT INTERRUPT FUNCTION. 
 
Hint: The INTÌ…Ì…Ì…Ì…Ì… signal is issued when EIE bit is set to 1. The EVF flag is set to 1 to indicate that an external event has 
occurred. 
 
Caution: For the Time Stamp EVI function, only the TS EVI Count register is responsible for detecting first or last 
event (EVOW), and therefore, always after an overflow of the TS EVI Count register from 255 to 0, a new First Event 
is allowed by the function (see also TIME STAMP EVI SCHEME).
```

## Page 102

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 102/154 Rev. 1.3 
4.17.1. TIME STAMP EVI SCHEME 
Time Stamp EVI scheme: 
 
EXT. EVENT
FUNCTION
EHL, ET
AND
&VBACKUP
Power state
(7)
EVI
interface: (6)
read only
interface:
read/write
capture/copy Time Stamp
Month
Year
Hours
Date
Seconds
Minutes
TS EVI Date
TS EVI Month
TS EVI Minutes
TS EVI Hours
TS EVI 100th Secs.
TS EVI Seconds
TS EVI Count
clear
0
EVOW
1
(1)
1 last event
first event
(3)
AND
& OR
 1
enable
overwrite TS EVI Year
100th Seconds interface:
read only
0
1
(2)
1
TS EVI Count = 00h
EVENT FLAG
EVF
SET
0
EIE
1
CLEARfrom interface:
clear EVF
to interface:
read EVF
EI
INTDE, CEIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON 
OR
 1
INT
0
EVR
11
0
STOP
11 stop
0
ESYN
1
clearauto
(5)
OR
 1
0
11
write to
Seconds reg.
(4)
 
 
(1) When EVOW bit is set to 1 the TS EVI registers (TS EVI 100th Seconds to TS EVI Year) are overwritten. 
The last occurred event is recorded. When EVOW bit is set to 0, the TS EVI registers are overwritten 
once only. To initialize or reinitialize the first event function, the Time Stamp EVI registers have to be 
cleared by writing 1 to the EVR bit (POR has same effect, when EVI pin = HIGH).  The TS EVI Count 
register always counts events, regardless of the settings of the override bit EVOW.  
Caution: After overflow of the TS EVI Count register from 255 to 0, a new First Event is performed. 
(2) If TS EVI Count register = 00h, a first event can be timestamped. 
(3) When writing 1 to EVR bit, all eight time stamp registers (TS EVI Count to TS EVI Year) are reset to 00h. 
Bit EVR can be left at 1. Writing or overwriting a 1 causes reset. 
(4) When an External Event occurs, the Time Stamp EVI is created first and then the 100 th Seconds register 
is cleared to 00. After the event detection, the ESYN bit is reset to 0 automatically. 
(5) Changing STOP bit from 1 to 0, or writing to the Seconds register, or when the ESYN bit is 1 in case of an 
External Event detection on EVI pin do not create an extra 1 Hz tick for the Seconds register. 
(6) During I2C read access to the TS EVI registers the time stamp capture function is blocked. 
(7) In VBACKUP Power state, the External Event function is disabled. 
 
See also Interrupt Scheme (part 2/3) in section INTERRUPT SCHEME.
```

## Page 103

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 103/154 Rev. 1.3 
4.18. TIME STAMP TLOW FUNCTION 
The Time Stamp TLow function is enabled by the control bit TLE. 
If TEMP [11:4] < TLT is detected (automatic temperature measurement is carried out once per second), the clock 
and calendar registers are captured and copied into the Time Stamp TLow registers. When the TLOW bit is set to 0 
and if the Time Stamp TLow registers were previously initialized to zero by writing 1 to the TLR bit, only one (the first) 
event is recorded. When the TLOW bit is set to 1, the last occurred event is recorded and TS TLow registers (TS 
TLow Seconds to TS TLow Year) are overwritten. The TS TLow Count register always counts events, regardless of 
the settings of the override bit TLOW. 
When writing 1 to TLR bit, all seven time stamp registers (TS TLow Count to TS TLow Year) are reset to 00h and the 
TLF flag is automatically cleared to 0. The TLR bit always returns 0 when read. Hint: All Time Stamp TLow registers 
are also reset to 00h at POR. Before using the Time Stamp TLow function, it is recommended to write 1 to TLR bit. 
When the STOP bit value is 1, the Temperature Low Interrupt function cannot provide new data because the 1 Hz 
tick is stopped and because temperature measurement, temperature compensation and temperature comparison 
with the TLT value is stopped. When writing to the Seconds register or when the ESYN bit is 1 in case of an External 
Event detection on EVI pin the time is synchronized and the 100 th Seconds register is reset to 00 (see TIME 
SYNCHRONIZATION). 
 
Procedure for using the Time Stamp TLow function: 
1. Initialize bits TLE and TLIE to 0. 
2. Select TLOW (0 or 1) and clear TLF flag (or automatically done in the following step). 
3. Write 1 to TLR bit, to reset all Time Stamp TLow registers to 00h. The TLF flag is automatically cleared to 0 
when writing 1 to the TLR bit. Bit TLR always returns 0 when read. 
4. Initialize the TEMPERATURE LOW INTERRUPT FUNCTION. 
5. Set the TLE bit to 1 to enable the Time Stamp TLow function. 
 
Hint: The INTÌ…Ì…Ì…Ì…Ì… signal is issued when TLIE bit is set to 1. The TLF flag is set to 1 to  indicate that a temperature low 
event has occurred. 
 
Hint: For the Time Stamp TLow function, all TS TLow registers are responsible for detecting first or last event (TLOW). 
When all TS TLow registers are 00h a First Event is allowed by the function. See a lso the following scheme: 
 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 104

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 104/154 Rev. 1.3 
4.18.1. TIME STAMP TLOW SCHEME 
Time Stamp TLow scheme: 
 
TLOW FLAG
    TLF (6)
SET
0
TLIE
1
CLEAR
to interface:
read TLF
TLI
INTDE, CTLIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON CLEAR
TLT
TEMP [11:4]
<
0
TLE
1
check once
per second
interface: (5)
read only
interface:
read/write
capture/copy Time Stamp
Month
Year
Hours
Date
Seconds
Minutes
TS TLow Month
TS TLow Year
TS TLow Hours
TS TLow Date
TS TLow Seconds
TS TLow Minutes
TS TLow Count
clear
0
TLOW
1
(1)
1 last event
first event
(3)
AND
& OR
 1
enable
overwrite
0
1
(2)
1
all TS TLow = 00h
OR
 1
INT
0
TLR
11
0
STOP
11
stop
auto
(4)
from interface:
clear TLF
clear THF
 
 
(1) When TLOW bit is set to 1 the TS TLow registers (TS TLow Seconds to TS TLow Year) are overwritten. 
The last occurred event is recorded. When TLOW bit is set to 0, the TS TLow registers are overwritten 
once only. To initialize or reinitialize the first event function, the Time Stamp TLow registers have to be 
cleared by writing 1 to the TLR bit (POR has same effect). The TS TLow Count register always counts 
events, regardless of the settings of the override bit TLOW. 
(2) If all TS TLow register = 00h, a first event can be timestamped. 
(3) When writing 1 to TLR bit, all seven time stamp registers (TS TLow Count to TS TLow Year) are reset to 
00h and the TLF flag is automatically cleared to 0. The TLR bit always returns 0 when read.  
(4) Changing STOP bit from 1 to 0, or writing to the Seconds register, or when the ESYN bit is 1 in case of an 
External Event detection on EVI pin do not create an extra 1 Hz tick for the Seconds register. 
(5) During I2C read access to the TS TLow registers the time stamp capture function is blocked. 
(6) Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s 
or 1s). 
 
See also Interrupt Scheme (part 2/3) in section INTERRUPT SCHEME.
```

## Page 105

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 105/154 Rev. 1.3 
4.19. TIME STAMP THIGH FUNCTION 
The Time Stamp THigh function is enabled by the control bit THE. 
If TEMP [11:4] > THT is detected (automatic temperature measurement is carried out once per second), the clock 
and calendar registers are captured and copied into the Time Stamp THigh registers. When the THOW bit is set to 0 
and if the Time Stamp THigh registers were previously initialized to zero by writing 1 to the THR bit, only one (the 
first) event is recorded. When the THOW bit is set to 1, the last occurred event is recorded and TS THigh registers 
(TS THigh Seconds to TS THigh Year) are overwritten.  The TS THigh Count register always counts events, 
regardless of the settings of the override bit THOW. 
When writing 1 to THR bit, all seven time stamp registers (TS THigh Count to TS THigh Year) are reset to 00h and 
the THF flag is automatically cleared to 0. The THR bit always returns 0 when read.  Hint: All Time Stamp THigh 
registers are also reset to 00h at POR. Before using the Time Stamp THigh function, it is recommended to write 1 to 
THR bit. 
When the STOP bit value is 1, the Temperature High Interrupt function cannot provide new data because the 1 Hz 
tick is stopped and because temperature measurement, temperature compensation and temperature comparison 
with the THT value is stopped. When writing to the Seconds register or when the ESYN bit is 1 in case of an External 
Event detection on EVI pin the time is synchronized and the 100 th Seconds register is reset to 00 (see TIME 
SYNCHRONIZATION). 
 
Procedure for using the Time Stamp THigh function: 
1. Initialize bits THE and THIE to 0. 
2. Select THOW (0 or 1) and clear THF flag (or automatically done in the following step). 
3. Write 1 to THR bit, to reset all Time Stamp THigh registers to 00h. The THF flag is automatically cleared to 
0 when writing 1 to the THR bit. Bit THR always returns 0 when read. 
4. Initialize the TEMPERATURE HIGH INTERRUPT FUNCTION. 
5. Set the THE bit to 1 to enable the Time Stamp THigh function. 
 
Hint: The INTÌ…Ì…Ì…Ì…Ì… signal is issued when THIE bit is set to 1. The THF flag is set to 1 to  indicate that a temperature high 
event has occurred. 
 
Hint: For the Time Stamp THigh function, all TS THigh registers are responsible for detecting first or last event 
(THOW). When all TS THigh registers are 00h a First Event is allowed by the function. See also the following scheme: 
 
Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s or 1s).
```

## Page 106

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 106/154 Rev. 1.3 
4.19.1. TIME STAMP THIGH SCHEME 
Time Stamp THigh scheme: 
 
THIGH FLAG
    THF (6)
SET
0
THIE
1
CLEAR
to interface:
read THF
THI
INTDE, CTHIE
INTERRUPT 
DELAY 
AFTER 
CLKOUT ON CLEAR
THT
TEMP [11:4]
>
0
THE
1
check once
per second
interface: (5)
read only
interface:
read/write
capture/copy Time Stamp
Month
Year
Hours
Date
Seconds
Minutes
TS THigh Month
TS THigh Year
TS THigh Hours
TS THigh Date
TS THigh Seconds
TS THigh Minutes
TS THigh Count
clear
0
THOW
1
(1)
1 last event
first event
(3)
AND
& OR
 1
enable
overwrite
0
1
(2)
1
all TS THigh = 00h
OR
 1
INT
0
THR
11
0
STOP
11
stop
auto
(4)
from interface:
clear THF
clear TLF
 
 
(1) When THOW bit is set to 1 the TS THigh registers (TS THigh Seconds to TS THigh Year) are overwritten. 
The last occurred event is recorded. When THOW bit is set to 0, the TS THigh registers are overwritten 
once only. To initialize or reinitialize the first event function, the Time Stamp THigh registers have to be 
cleared by writing 1 to the THR bit (POR has same effect). The TS THigh Count register always counts 
events, regardless of the settings of the override bit THOW. 
(2) If all TS THigh register = 00h, a first event can be timestamped. 
(3) When writing 1 to THR bit, all seven time stamp registers (TS THigh Count to TS THigh Year) are reset to 
00h and the THF flag is automatically cleared to 0. The THR bit always returns 0 when read. 
(4) Changing STOP bit from 1 to 0, or writing to the Seconds register, or when the ESYN bit is  1 in case of an 
External Event detection on EVI pin do not create an extra 1 Hz tick for the Seconds register. 
(5) During I2C read access to the TS THigh registers the time stamp capture function is blocked. 
(6) Note that the THF and TLF flags are always reset whenever the register 0Dh Status is written to (using 0s 
or 1s). 
 
See also Interrupt Scheme (part 2/3) in section INTERRUPT SCHEME.
```

## Page 107

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 107/154 Rev. 1.3 
4.20. TEMPERATURE REFERENCE ADJUSTMENT 
The Temperature Reference 16-bit value TREF can be used to calibrate the Temperature value TEMP of the Digital 
Thermometer. The adjustment of TREF is purely digital  and has only the effect of shifting the linear curve of the 
thermometer vertically up or down. This is a one-point setting and is usually made at room temperature for post PCB 
soldering temperature variation compensation. The change in TREF has no effect on the temperature compensation 
of the RTC. 
The TREF value contains a two's complement number with a minimum adjustment step (one LSB) of  ±1/128 = 
±0.0078125°C. The preconfigured (Factory Calibrated) TREF value may be changed by the user  (see EEPROM 
TEMPERATURE REFERENCE REGISTERS). Note the following special formulas for conversion to °C and back. 
 
 
4.20.1. TREF VALUE DETERMINATION 
The following registers and fields are related to the Temperature Reference value TREF: 
 
? TEMP [3:0] field (see TEMPERATURE REGISTERS, 0Eh - Temperature LSBs) 
? TEMP [11:4] field (see TEMPERATURE REGISTERS, 0Fh - Temperature MSBs) 
? TREF [7:0] field (see EEPROM TEMPERATURE REFERENCE REGISTERS, C4h - EEPROM 
TReference 0) 
? TREF [15:8] field (see EEPROM TEMPERATURE REFERENCE REGISTERS, C5h - EEPROM 
TReference 1) 
 
See also EEPROM READ/WRITE. 
 
Formulas for conversion from 16-bit TREF value to TREF value in °C and back (see also table below): 
 
TREF in °C = ( TREF
128  - 0.5)°C 
 
TREF =  (TREF in °C + 0.5°C) × 128
°C  
 
A new 16-bit reference value TREF is determined by the following process: 
1. Make an exact temperature measurement Ttarget in °C with an external temperature measurement device 
at room temperature and read out the 12-bit TEMP value of the temperature registers at the same time. 
The measurement sensor for Ttarget must be as close as possible to the RTC. 
2. Convert the 12-bit TEMP value into the TEMP value in °C (see TEMPERATURE REGISTERS). 
3. Calculate the temperature difference ∆T = Ttarget - TEMP, all in °C. 
4. Read the 16-bit TREF value. 
5. Convert the TREF value in °C (see formula above). 
6. Calculate the new TREF = TREF + ∆T, all in °C. 
7. Convert the new TREF value in °C into the 16-bit TREF value (see formula above). 
8. Write the new TREF value to the registers. 
 
Example: 
1. Ttarget = 26°C, TEMP = 384d 
2. TEMP = 384/16 = 24°C 
3. ∆T = Ttarget - TEMP = 26°C - 24°C = +2°C 
4. TREF = 3059d 
5. TREF = (3059/128 - 0.5)°C = 23.3984375°C 
6. New TREF = TREF + ∆T = 23.3984375 °C + 2°C = 25.3984375°C 
7. New TREF = (new TREF in °C + 0.5°C) × 128/°C = (25.3984375°C + 0.5°C) × 128/°C = 3315d 
8. New TREF = 3315d
```

## Page 108

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 108/154 Rev. 1.3 
TREF (16 bits) examples: 
TREF [15:0] value Hexadecimal Decimal (Signed decimal) TREF in °C(*) 
0001'0001'1100'0000 11C0 4544 4544 35 
0001'0001'1011'1111 11BF 4543 4543 34.9921875 
: : : : : 
0000'1100'1100'0001 0CC1 3265 3265 25.0078125 
0000'1100'1100'0000 0CC0 3264 3264 25 
0000'1100'1011'1111 0CBF 3263 3263 24.9921875 
: : : : : 
0000'0111'1100'0001 07C1 1985 1985 15.0078125 
0000'0111'1100'0000 07C0 1984 1984 15 
(*) TREF value in °C = (Signed decimal / 128) - 0.5 = (Signed decimal × 0.0078125) - 0.5. 
 Note that the TREF value (a two’s complement value) is used for the calibration of the room temperature value, and not to create a 
 special temperature offset. Therefore, the range of examples is only in the positive range, and only from +15°C to +35°C.
```

## Page 109

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 109/154 Rev. 1.3 
4.21. TIME SYNCHRONIZATION 
The time of the RV -3032-C7 can be synchronized in three ways: With the STOP bit, where the 1 Hz tick can be 
stopped completely, or by writing to the Seconds register, or by the ESYN bit, where the time is synchronized to an 
external signal. 
 
 
4.21.1. STOP BIT FUNCTION 
The STOP bit function is used for a software-based accurate and safe starting of the time circuits. 
 
The STOP bit is set to 1, the clock prescaler frequencies from 4096 Hz to 1 Hz are stopped and reset and thus no 1 
Hz ticks are generated and a possible currently memorized 1 Hz update is also reset. The 100 th Seconds register 
(100 Hz) is reset to 0 also. Because the upper stage of the prescaler is not reset and not stopped (8192 Hz) and the 
I2C interface is asynchronous, the first 1 Hz period after reset (after setting STOP bit from 1 to 0) will be 0 to 244 Î¼s 
shorter than 1 second. Stopping and resetting the prescaler will stop all subsequent peripherals (clock and calendar 
with alarm , XTAL CLKOUT, tim er clock, update timer clock, EVI input filter  and temperature measurement, 
temperature compensation and temperature comparison with THT and TLT values . The External Event Interrupt 
function is still working but cannot provide useful data. 
The STOP bit fun ction will not affect the CLKOUT of 32.768 kHz (see also XTAL CLKOUT FREQUENCY 
SELECTION). 
 
Scheme of the STOP bit function: 
 
XTAL
OSCILLATOR F0 F1 F2 F13 F14
RESET RESET RESET
32.768 kHz
16.384 kHz
8192 Hz
4096 Hz
2 Hz
1 Hz tick
STOP
1 Hz
64 Hz
1024 Hz
32.768 kHz
CLKOUT
source
0
1
 
 
 
When STOP bit is set, the time registers can be set and do not increment until the STOP bit is released. 
 
Setting the clock and calendar values using the STOP bit function: 
1. Set STOP bit to 1 to prevent a timer update while setting the time. 
2. Write the desired clock and calendar values to the registers (year, month, dat e, weekday, hours, minutes 
and seconds). The 100th Seconds register is automatically cleared to 00 when setting the STOP bit to 1.  
3. Release STOP bit to 0 to start the time circuits. 
4. The first 1 Hz period is started at the I2C Acknowledge from RV-3032-C7 after writing to the Control 2 
register.
```

## Page 110

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 110/154 Rev. 1.3 
Timing of the STOP bit function: 
 
STOP
Seconds
1
Write operation
1 Hz
CLKOUT
A B B+1 B+2 C C+1
1 s1 s - 0 to 244 µs
3
1 s - 0 to 244 µs
4
1 s
2 2
 
 
1
 To monitor the synchronicity of the 1 Hz tick to an external clock source, the 1 Hz clock can be enabled  
  on CLKOUT pin. The positive edge corresponds to the 1 Hz tick for the clock counter increment (except for  
  the possible positive edge when the STOP bit is cleared). 
2
 As long as the STOP bit is set to 1, new time and date register values can be entered. 
3
 When changing STOP bit from 1 to 0 an immediate positive edge on the LOW signal on CLKOUT pin is 
  created. The first 1 Hz period after reset will be 0 to 244 µs shorter than 1 second.  
4
 When changing STOP bit from 1 to 0 the HIGH signal on CLKOUT pin does not change. The first 1 Hz 
  period after reset will be 0 to 244 µs shorter than 1 second.
```

## Page 111

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 111/154 Rev. 1.3 
4.21.2. WRITING TO SECONDS REGISTER 
Writing to the Seconds register is used for a software -based accurate and safe starting of the time circuits 
(synchronization). 
When writing to the Seconds register, the clock prescaler frequencies from 4096 Hz to 1 Hz are reset and a possible 
currently memorized 1 Hz update is also reset. The 100th Seconds register (100 Hz) is reset to 0 also. Because the 
upper stage of the prescaler is not reset (8192 Hz) and the I 2C interface is asynchronous, the first 1 Hz period after 
reset will be 0 to 244 Î¼s shorter than 1 second. Resetting the prescaler affects the length of the current clock period 
of all subsequent peripherals  (clock and calendar, XTAL CLKOUT, timer clock, update timer clock, temperature 
sensing and EVI input filter). 
Writing to the Seconds register will not affect the CLKOUT of the 32.768 kHz (see also XTAL CLKOUT FREQUENCY 
SELECTION). 
 
Scheme of the reset function when writing to the Seconds register: 
 
XTAL
OSCILLATOR F0 F1 F2 F13 F14
RESET RESET RESET
32.768 kHz
16.384 kHz
8192 Hz
4096 Hz
2 Hz
1 Hz tick
1 Hz
64 Hz
1024 Hz
32.768 kHz
CLKOUT
source 0
11
write to
Seconds reg.
 
 
 
Setting the clock and calendar values using the reset function when writing to the Seconds register: 
1. Write the desired clock and calendar values (within 950 ms) to the registers (seconds, minutes, hours, 
weekday, date, month and year). 
2. The first 1 Hz period is started at the I2C Acknowledge from RV-3032-C7 after writing to the Seconds register. 
 
Timing of the reset function when writing to the Seconds register: 
 
write to
Seconds reg.
Seconds
1
Write operation
1 Hz
CLKOUT
A B B+1 B+2 C C+1
1 s1 s - 0 to 244 µs
2
1 s - 0 to 244 µs
3
1 s
A+1
 
 
1
 To monitor the synchronicity of the 1 Hz tick to an external clock source, the 1 Hz clock can be enabled  
  on CLKOUT pin. The positive edge corresponds to the 1 Hz tick for the clock counter increment (except for 
  the possible positive edge when writing to the Seconds register). 
2
 Writing to the Seconds register creates an immediate positive edge on the LOW signal on CLKOUT pin. The  
  first 1 Hz period after reset will be 0 to 244 µs shorter than 1 second. 
3
 Writing to the Seconds register does not change the HIGH signal on CLKOUT pin. The first 1 Hz period after  
  reset will be 0 to 244 µs shorter than 1 second.
```

## Page 112

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 112/154 Rev. 1.3 
4.21.3. ESYN BIT FUNCTION 
The External Event (EVI) Synchronization bit ESYN is used for an external event triggered highly accurate time 
adjustment (synchronizing). 
If the ESYN bit is 1 and in case of an External Event detection on the EVI pin, the clock prescaler frequencies fr om 
4096 Hz to 1 Hz are reset and a possible currently memorized 1 Hz update is also reset. The 100th Seconds register 
(100 Hz) is reset to 0 also. Because the upper stage of the prescaler is not reset (8192 Hz) and the I 2C interface is 
asynchronous, the first 1 Hz period after reset will be 0 to 244 Î¼s shorter than 1 second. Resetting the prescaler 
affects the length of the current clock period of all subsequent peripherals (clock and calendar, XTAL CLKOUT, timer 
clock, update timer clock, temperature sensing and EVI input filter). After the event detection, the ESYN bit is reset 
to 0 automatically. 
The external triggered time adjustment will not affect the CLKOUT of the 32.768 kHz (see also XTAL CLKOUT 
FREQUENCY SELECTION). 
 
Scheme of the ESYN bit function: 
 
XTAL
OSCILLATOR F0 F1 F2 F13 F14
RESET RESET RESET
32.768 kHz
16.384 kHz
8192 Hz
4096 Hz
2 Hz
1 Hz tick
1 Hz
64 Hz
1024 Hz
32.768 kHz
CLKOUT
source
EXT. EVENT
FUNCTION
EHL, ET
EVI
0
ESYN
1
auto
 
 
 
Setting the clock and calendar values synchronous to an External Event detection on EVI pin:  
 
1. Initialize the External Event Function according to USE OF THE EXTERNAL EVENT INTERRUPT with bit 
ESYN set to 1. 
2. When interrupt pin INTÌ…Ì…Ì…Ì…Ì… is triggered by the External Event Function, write the desired clock and calendar 
values to the registers (year, month, date, weekday, hours, min utes and seconds). The 100 th Seconds 
register is cleared to 00 automatically. 
Note that when you write to the Seconds register, the time is synchronized again. 
3. After the event detection, the ESYN bit is reset to 0 automatically. 
 
See also EXTERNAL EVENT INTERRUPT FUNCTION. 
 
Hint: The ESYN bit function does first the Time Stamp EVI, then clears 100 th Seconds register. 
Hint: If ESYN is 1, the synchronization function can be cancelled at any time by resetting the ESYN bit to 0.
```

## Page 113

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 113/154 Rev. 1.3 
Timing of the ESYN bit function: 
 
ESYN
Seconds
1
Write operation
1 Hz
CLKOUT
A A+2 A+3 A+4
1 s1 s - 0 to 244 µs 1 s - 0 to 244 µs 1 s
A+1
event
EVI 4
event
3
2 2
 
 
1
 To monitor the synchronicity of the 1 Hz tick to an external clock source, the 1 Hz clock can be enabled  
  on CLKOUT pin. The positive edge corresponds to the 1 Hz tick for the clock counter increment (except for 
  the possible positive edge when ESYN = 1 and an external event occurs). 
2
 To initialize the external event synchronization function, bit ESYN has to be set to 1. 
3
 If ESYN bit is 1 and an external event occurs, the ESYN bit is cleared automatically and an immediate 
  positive edge on the LOW signal on CLKOUT pin is created. The first 1 Hz period after reset will be 0 to 
  244 µs shorter than 1 second. 
4
 If ESYN bit is 1 and an external event occurs, the ESYN bit is cleared automatically and the HIGH signal on 
  CLKOUT pin does not change. The first 1 Hz period after reset will be 0 to 244 µs shorter than 1 second.
```

## Page 114

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 114/154 Rev. 1.3 
4.22. USER PROGRAMMABLE PASSWORD 
After a Power up and the first refreshment of tPREFR = ~66 ms , the Password PW registers (RAM 39h to 3Ch) are 
reset to 00h and the value in EEPWE (EEPROM CA h) and the values in the EEPROM Password EEPW registers 
(EEPROM C6h to C9h) are copied from EEPROM to the corresponding RAM mirror.  
The first four Password registers (PW), in case of the use of the function (enabled by writing 255 into the EEPROM 
Password Enable register EEPWE), are necessary to be able to write in all writable registers that have the convention 
WP (time, control, user RAM,  configuration EEPROM and user EEPROM  registers). The 32-Bit Password PW is 
compared with the 32 bits stored in the RAM mirror of the EEPW registers (see PASSWORD REGISTERS, EEPROM 
PASSWORD REGISTERS and EEPROM PASSWORD ENABLE REGISTER). 
 
Caution: The number of possible passwords is 232 ≈ 4.3 × 109 = 4.3 billion. 
 
 
4.22.1. ENABLE/DISABLE WRITE PROTECTION 
If the write protection function is enabled by writing 255 in register EEPWE (EEPROM CAh), it remains possible to 
read all the registers except the EEPROM registers. The EEPROM registers cannot be read because it cannot be 
written to the EE Address and EE Command registers. If the function is not enabled, read and write are possible for 
all corresponding registers. 
If the write protection function is enabled, it is necessary to first write the correct 32-Bit Password PW (PW = EEPW) 
(Unlock), before any attempt to write in the RAM registers and to read and write in the EEPROM registers.  
Once the user is finished with the write access and subsequently the write protection is still enabled or enabled again 
(by writing 255 in EEPROM register EEPWE), it is necess ary to write an incorrect password (PW ≠ EEPW) into the 
Password PW registers in order to write -protect (Lock) the registers. See program sequences below and 
FLOWCHART. 
 
Enable write protection: 
1. Initial state (POR): WP-Registers are Not write-protected (EEPWE ≠ 255) 
Reference password is stored in the RAM mirror of EEPW (addrs C6h to C9h) 
2. Disable automatic refresh by setting EERD = 1 
3. Enable password function by entering EEPWE = 255 (RAM mirror address CAh) 
4. Enter the correct password PW (PW = EEPW) to unlock write protection (RAM addresses 39h to 3Ch) 
5. Update EEPROM (all Configuration RAM ? EEPROM) by writing 11h to EECMD 
6. Enable automatic refresh by setting EERD = 0 
7. Enter an incorrect password PW (PW ≠ EEPW) to lock the device (RAM addresses 39h to 3Ch) 
8. Final state: WP-Registers are Write-protected by password (EEPWE = 255) 
 
Disable write protection: 
1. Initial state (POR): WP-Registers are Write-protected by password (EEPWE = 255) 
Reference password is stored in the RAM mirror of EEPW (addrs C6h to C9h) 
2. Enter the correct password PW (PW = EEPW) to unlock write protection (RAM addresses 39h to 3Ch) 
3. Disable automatic refresh by setting EERD = 1 
4. Disable password function by entering EEPWE ≠ 255) (RAM mirror address CAh) 
5. Update EEPROM (all Configuration RAM ? EEPROM) by writing 11h to EECMD 
6. Enable automatic refresh by setting EERD = 0 
7. Final state: WP-Registers are Not write-protected (EEPWE ≠ 255) 
 
Hint: The EEPROM values of the reference password in the EEPROM Password EEPW registers can be READ with 
the Read one EEPROM byte command ( writing 22h to EECMD ) when in unlocked state (registers not write -
protected). This option is useful if it is not certain which passwo rd is written in the EEPW before the write protection 
function is enabled. The RAM mirror from the EEPW registers can never be read.
```

## Page 115

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 115/154 Rev. 1.3 
4.22.2. CHANGING PASSWORD 
To code a new password, the user has to first enter the current (correct) Password PW (PW = EEPW) int o the 
registers 39h to 3Ch, if the WP-Registers are write protected, and then write a value not equal to all 1 (value ≠ 255) 
in the EEPWE register (EEPROM CAh) to unlock write protection, and then write the new reference password EEPW 
into the EEPROM registers C6h to C9h and writing all 1 (value = 255) in the EEPWE register to enable password 
function. See program sequences below and FLOWCHART. 
 
Change password if password function is enabled (EEPWE = 255): 
1. Initial state (POR): WP-Registers are Write-protected by old reference Password EEPW 
Reference password is stored in the RAM mirror of EEPW (addrs C6h to C9h) 
2. Enter old, correct password PW (PW = EEPW) to unlock write protection (RAM addresses 39h to 3Ch) 
3. Disable automatic refresh by setting EERD = 1 
4. Disable password function by entering EEPWE ≠ 255 (RAM mirror address CAh) 
5. Define a new reference password in the EEPW registers (RAM mirror addresses C6h to C9h) 
6. Enable the password function by entering EEPWE = 255 (RAM mirror address CAh) 
7. Enter new, correct password PW (PW = EEPW) to unlock write protection (RAM addresses 39h to 3Ch) 
8. Update EEPROM (all Configuration RAM ? EEPROM) by writing 11h to EECMD 
9. Enable automatic refresh by setting EERD = 0 
10. Enter an incorrect password PW (PW ≠ EEPW) to lock the device (RAM addresses 39h to 3Ch) 
11. Final state: WP-Registers are Write-protected by new reference EEPROM Password EEPW 
 
Change password if password function is disabled (EEPWE ≠ 255): 
1. Initial state (POR): Old reference password is stored in the EEPROM Password EEPW 
Reference password is stored in the RAM mirror of the EEPROM Password EEPW (addrs C6h to C9h) 
2. Disable automatic refresh by setting EERD = 1 
3. Define a new reference password in the EEPW registers (RAM mirror addresses C6h to C9h) 
4. Update EEPROM (all Configuration RAM ? EEPROM) by writing 11h to EECMD 
5. Enable automatic refresh by setting EERD = 0 
6. Final state: New reference password is stored in the EEPROM Password EEPW 
 
Note that the EEPROM password EEPW = 00000000h is not a real password, because after POR the password PW 
is also 00000000h (PW = EEPW) and although the password function is enabled  after POR refresh (EEPW = 255) 
the PW-Registers are unlocked.
```

## Page 116

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 116/154 Rev. 1.3 
4.22.3. FLOWCHART 
The following flowchart describes the programming of the enabling and disabling of the register write protection by 
user password and the changing of the user password and the other Configuration EEPROM registers (C0h to C5h) 
if write protection is enabled or disabled. In this example the Update EEPROM command ( writing 11h to EECMD) is 
applied to write (store) data from all Configuration RAM mirror bytes (addresses C0h to CAh) into the corresponding 
Configuration EEPROM bytes. See also USE OF THE CONFIGURATION REGISTERS. 
 
User programmable password flowchart: 
 
POR
Disable
automatic refresh
Enable
password function
EERD = 1
EEPWE = 255
(RAM)
Update
EEPROM
EECMD = 11h
(RAM ? EEPROM)
Enable
automatic refresh EERD = 0
Lock
POR
Unlock
EERD = 1 Disable
automatic refresh
Disable
password function
EEPWE   255
(RAM)
Update
EEPROM
EECMD = 11h
(RAM ? EEPROM)
Enable
automatic refreshEERD = 0
Define new 
password EEPW
(RAM)
(1)
EEPWE   255 (RAM)
(2)
EEPWE = 255 (RAM)
Enter incorrect PW
(PW   EEPW)
Enter correct PW 
(PW = EEPW)
Edit other
Config. EEPROM
(C0h to C5h)
(RAM)
WP-Registers are
Write-protected
Edit all RAM
(00h to 4Fh)
Edit other
Config. EEPROM
(C0h to C5h)
(RAM)
No write 
protection
Using write 
protection
WP-Registers are
Not write-protected
(3) (3)
 
 
(1) Entry point after POR refresh when EEPWE ≠ 255. 
(2) Entry point after POR refresh when EEPWE = 255. 
(3) If a new reference password has previously been defined in the EEPROM Password EEPW registers (RAM), 
  the new correct password PW (PW = EEPW) must be entered here in order to unlock the write protection.
```

## Page 117

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 117/154 Rev. 1.3 
5. TEMPERATURE COMPENSATION 
5.1. XTAL MODE FREQUENCIES 
Xtal 32.768 kHz 
The Xtal 32.768 kHz clock is not temperature compensated. Due to its negative temperature coefficient with a 
parabolic frequency deviation, a change of up to -150 ppm across the standard operating temperature range from -
40°C to +85°C can result (for the extended operating range of -40°C to +105°C a frequency change of -225 ppm can 
result). The 32.768 kHz oscillator frequency on all devices is tested not to exceed a frequency deviation of ±50 ppm 
(parts per million) at 25°C. 
 
Frequencies from 4096 Hz to 64 Hz 
These frequencies are digitally temperature compen sated with a Time Accuracy of ±2.5  ppm over the standard 
temperature range from -40°C to +85°C and of ±20 ppm for the extended temperature range from +85°C to +105°C. 
The clock at the 16.384 kHz level of the divider chain is modified by adding or subtracting 32.768 kHz level pulses. 
The pulses are added or subtracted according to the expected frequency deviation computed by the temperature 
compensation algorithm. The d igital compensation method (adding and subtracting clock pulses) is affecting the 
cycle-to-cycle jitter of the digitally compensated frequencies shown below.  
? 4096 Hz (Periodic Countdown Timer Interrupt) 
? 1024 Hz (CLKOUT) 
? 100 Hz  (External Event Interrupt) 
? 64 Hz  (CLKOUT and Periodic Countdown Timer Interrupt) 
Aging compensation can be done with the OFFSET value (see AGING CORRECTION). 
 
1 Hz and Clock / Calendar 
The 1 Hz clock is temperature compensated and using both, digital coarse compensation and digital fine adjustment. 
The Time Accuracy and the Frequency Accuracy is ±2.5 ppm for every 1 Hz period over the standard temperature 
range from -40°C to +85°C and ±20 ppm for the extended temperature range from +85°C to +105°C. The temperature 
compensation algorithm adjusts every 1 Hz period with a resolution of about 0.1 ppm.  This precise and accurate 1 
Hz clock is used to increment all subsequent clock and calendar registers  (see TIME ACCURACY VS. 
TEMPERATURE CHARACTERISTICS). 
Aging compensation can be done with the OFFSET value (see AGING CORRECTION). 
 
 
5.2. COMPENSATION VALUES 
Each device is factory calibrated over the full temperature range, and the individual compensation values are stored 
in the EEPROM of the Digital Temperature Compensation Unit (DTCU). This EEPROM is not accessible for the user.
```

## Page 118

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 118/154 Rev. 1.3 
5.3. AGING CORRECTION 
An aging adjustment or accuracy tuning can be done with the OFFSET value. The correction is purely digital and has 
only the effect of shifting the time vs. temperature curve vertically up or down. It has no effect on the time vs. 
temperature characteristics of the final frequency.  The OFFSET value contains a two 's complement number with a 
range of -32 to +31 adjustment steps.  The minimal correction step (one LSB) i s ±1/(32768 × 128) = ±0.2384 ppm. 
The maximum correction range is roughly ±7.4 ppm. Note that the signed offset value OFFSET corresponds to the 
actual offset value of the measured frequency. The user has access to this field (see EEPROM OFFSET REGISTER). 
 
 
5.3.1. OFFSET VALUE DETERMINATION 
The OFFSET value is determined by the following process: 
 
1. Set the OFFSET field to 0 to ensure correction is not occurring. 
2. Select the 1 Hz frequency on the CLKOUT pin. 
3. Measure the frequency Fmeas at the output pin in Hz. See 
MEASURING TIME ACCURACY AT CLKOUT PIN. 
4. Compute the offset value required in ppm: POffset = ((Fmeas - 1) × 1’000’000) 
5. Compute the offset value in steps: Offset = POffset/(1/(32768 × 128)) = POffset/(0.2384) 
6. If Offset > 31, the frequency is too high to be corrected. 
7. Else if 0 ≤ Offset ≤ 31, set OFFSET = Offset 
8. Else if -32 ≤ Offset ≤ -1, set OFFSET = Offset + 64 
9. Else the frequency is too low to be corrected. 
 
Examples: 
? If 1.0000012 Hz is measured when the 1 Hz clock is selected, the offset is +0.0000012 Hz, which is 
+0.0000012 Hz / 10-6 Hz = +1.2 ppm. The positive offset value is then calculated as follows: 
+1.2 ppm / 0.2384 ppm = +5.03, the rounded integral part is +5. In binary, OFFSET = 000101.  
 
? If 0.9999949 Hz is measured when the 1 Hz clock is selected, the offset is -0.0000051 Hz, which is 
-0.0000051 Hz / 10-6 Hz = -5.1 ppm. The negative offset value is then calculated as follows: 
-5.1 ppm / 0.2384 ppm = -21.39, the rounded integral part is -21. The unsigned value is then: 
-21 +64 = +43. In binary, OFFSET = 101011.
```

## Page 119

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 119/154 Rev. 1.3 
5.3.2. MEASURING TIME ACCURACY AT CLKOUT PIN 
The simplest method to verify the time accuracy of the Digital Temperature Compensation Unit (DTCU) is to measure 
the compensated 1 Hz frequency at the CLKOUT pin. The 1 Hz clock frequency contains di gitally temperature 
compensated clocks with digital fine adjustment and represents the overall time accuracy of the device. 
 
1. Select the 1 Hz frequency at CLKOUT pin: 
a. Set OS bit to 0 to select XTAL mode (EEPROM C3h). 
b. Set FD field to 11 to select 1 Hz (EEPROM C3h). 
c. Set NCLKE bit to 0 to directly enable square wave output on CLKOUT pin. 
 
2. Measuring equipment and setup: 
a. Use a high-precision universal counter to observe the 1 Hz time accuracy on CLKOUT pin. 
b. Trigger on the rising edge of the hybrid signal (gate time ≥ 1 second). Each 1 Hz clock measured at 
the rising edge fully representing the accuracy of the DTCU. 
 
1 Hz time accuracy at CLKOUT pin (hybrid signal): 
 
CLKOUT
1 second
~500 ms
21
 
1
CLKOUT Output is active HIGH. 
  When measuring the time accuracy it is mandatory to trigger on the rising edge of the CLKOUT signal.  
  The temperature compensation algorithm adjusts every 1 Hz period with a resolution of about 0.1 ppm.  
2
The falling edge of the CLKOUT signal is generated when the RV-3032-C7 clears the signal after ~500 ms. 
  The negative edge is created by the 32.768 kHz Xtal and must not be used to test the time accuracy. 
 
 
Note that the Periodic Time Update Interrupt function (1 Second or 1 Minute  Update) as well as the other interrupt 
functions cannot be used for short-term time accuracy measurement as rising and falling edges of the INTÌ…Ì…Ì…Ì…Ì… signal are 
generated by the 32.768 kHz Xtal signal.
```

## Page 120

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 120/154 Rev. 1.3 
5.4. CLOCKING SCHEME 
Clocking Scheme with CLKOUT and Interrupts: 
 
4096 Hz Periodic Countdown Timer Interrupt
1024 Hz
CLKOUT
100 Hz External Event Interrupt
64 Hz
DTCU
coarse
Time Accuracy
±2.5 ppm
-40 to +85°C
(±20 ppm
+85 to +105°C)
OFFSET
STOP
Seconds
ESYN
CLKOUT32.768 kHzXtal
oscillator
HF oscillator CLKOUT8192 Hz to 67.109 MHz
100th Secs.
CLKOUT
Periodic Countdown Timer Interrupt
Divider
&
Counter
Divider
&
Counter
1 Hz
Minutes
Hours, Date
DTCU
fine
Time Accuracy
±2.5 ppm
-40 to +85°C
(±20 ppm
+85 to +105°C)
Seconds
Month, Year
CLKOUT
Periodic Countdown Timer Interrupt
Periodic Time Update Interrupt
External Event Interrupt
Temperature Low Interrupt
Temperature High Interrupt
Periodic Countdown Timer Interrupt
Periodic Time Update Interrupt
Alarm Interrupt
External Event Interrupt
Temperature Low Interrupt
Temperature High Interrupt
Alarm Interrupt
External Event Interrupt
Temperature Low Interrupt
Temperature High Interrupt
External Event Interrupt
Temperature Low Interrupt
Temperature High Interrupt
```

## Page 121

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 121/154 Rev. 1.3 
6. I2C INTERFACE 
The I2C interface is for bidirectional, two -line communication between different ICs or modules. The RV-3032-C7 is 
accessed at addresses A2h/A3h, and supports Fast Mode (up to 400 kHz). The I 2C interface consists of two lines: 
one bi-directional data line (SDA) and one clock line (SCL). Both lines are connected to a positive supply via pull -up 
resistors. Data transfer is initiated only when the interface is not busy.  
 
 
6.1. BIT TRANSFER 
One data bit is transferred during each clock pulse. The data on the SDA line remains stable during the HIGH period 
of the clock pulse, as changes in the data line at this time are interpreted as a control signals. Data changes should 
be executed during the LOW period of the clock pulse (see Figure below).  
 
Bit transfer: 
 
SDA
SCL
change of 
data 
allowed
data line 
stable;
data valid
 
 
 
 
6.2. START AND STOP CONDITIONS 
Both data and clock lines remain HIGH when the bus is not  busy. A HIGH-to-LOW transition of the data line, while 
the clock is HIGH, is defined as the START condition (S). A LOW -to-HIGH transition of the data line, while the clock 
is HIGH, is defined as the STOP condition (P) (see Figure below). 
 
Definition of START and STOP conditions: 
 
SDA
SCL
S
START condition
P
STOP condition
SDA
SCL
 
 
 
A START condition which occurs after a previous START but before a STOP is called a Repeated START condition, 
and functions exactly like a normal STOP followed by a normal START. 
 
Caution: 
When communicating with the RV-3032-C7 module, the series of operations from transmitting the START condition 
to transmitting the STOP condition should occur within 950 ms. 
 
If this series of operations requires 950 ms or longer, the I2C-bus interface will be automatically cleared and set to 
standby mode by the bus timeout function of the RV-3032-C7. Note with caution that both write and read operations 
are invalid for communications that occur during or after this auto clearing operation. When writing: no acknowledge 
will occur. When reading: FFh will be read. 
Restarting of communications begins with transfer of the START condition again.  
The I2C auto increment Address Pointer is neither reset by the I 2C STOP condition nor by the internal stop forced 
after timeout.
```

## Page 122

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 122/154 Rev. 1.3 
6.3. DATA VALID 
After a START condition, SDA is stable for the duration of the high period of SCL. The data on SDA may be changed 
during the low period of SCL. There is one clock pulse per bit of data. Each data transfer is initiated with a START 
condition and terminated with a STOP co ndition. The number of data bytes transferred between the START and 
STOP conditions is not limited (however, the transfer time must be no longer than 950 ms). The inf ormation is 
transmitted byte-wise and each receiver acknowledges with a ninth bit. 
 
 
6.4. SYSTEM CONFIGURATION 
Since multiple devices can be connected with the I2C-bus, all I2C-bus devices have a fixed and unique device address 
built-in to allow individual addressing of each device. 
The device that controls the I2C-bus is the Master; the devices which are controlled by the Master are the Slaves. A 
device generating a message is a Transmitter; a device receiving a message is the Receiver. The RV-3032-C7 acts 
as a Slave-Receiver or Slave-Transmitter. 
Before any data is transmitted on the I2C-bus, the device which should respond is addressed first. The addressing is 
always carried out with the first byte transmitted after the START procedure. The clock signal SCL is only an input 
signal, but the data signal SDA is a bidirectional line. 
 
System configuration: 
 
MASTER
TRANSMITTER
RECEIVER
SLAVE
RECEIVER
SLAVE
TRANSMITTER
RECEIVER
MASTER
TRANSMITTER
MASTER
TRANSMITTER
RECEIVER
SDA
SCL
```

## Page 123

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 123/154 Rev. 1.3 
6.5. ACKNOWLEDGE 
The number of data bytes transferred between the START and STOP conditions from transmitter to receiver is 
unlimited (however, the transfer time must be no longer than 950 ms). Each byte of eight bits is followed by an 
acknowledge cycle. 
? A slave receiver, which is addressed, must generate an acknowledge cycle after the reception of each 
byte. 
? Also a master receiver must generate an acknowledge cycle after the reception of each byte that has been 
clocked out of the slave transmitter. 
? The device that acknowledges must pull-down the SDA line during the acknowledge clock pulse, so that 
the SDA line is stable LOW during the HIGH period of the acknowledge-related clock pulse (set-up and 
hold times must be taken into consideration). 
? A master receiver must signal an end of data to the transmitter by generating a not-acknowledge cycle on 
the last byte that has been clocked out of the slave. In this event the transmitter must leave the data line 
HIGH to enable the master to generate a STOP condition. 
 
Data transfer and acknowledge on the I2C-bus: 
 
SCL from 
master 1 2 8 9
S
START
condition
data output
by receiver
data output
by transmitter
not-acknowledge
acknowledge
clock pulse for
acknowledgement
```

## Page 124

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 124/154 Rev. 1.3 
6.6. SLAVE ADDRESS 
On the I 2C-bus the 7 -bit slave address 1010001b (51h) is reserved for the RV-3032-C7. The entire I 2C-bus slave 
address byte is shown in the following table. 
 
Slave address R/W Transfer data 
Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0 
1 0 1 0 0 0 1 1 ( R ) A3h (read) 
0 ( W ) A2h (write) 
 
After a START condition, the I 2C slave address has to be sent to the RV-3032-C7 device. The R/W bit defines the 
direction of the following single or multiple byte data transfer. The 7-bit address is transmitted MSB first. If this address 
is 1010001b (51h), the RV-3032-C7 is selected, the eighth bit indicates a read (R/W = 1) or a write (R/W = 0) operation 
(results in A3 h or A 2h) and the RV-3032-C7 supplies the ACK. The RV-3032-C7 ignores all other address values 
and does not respond with an ACK. 
In the write operation, a data transfer is terminated by sending either the STOP condition or the START condition of 
the next data transfer. 
 
 
6.7. WRITE OPERATION 
Master transmits to Slave-Receiver at specified address. The Register Address is an 8 -bit value that defines which 
register is to be accessed next. After writing one byte, the Register Address is automatically incremented by 1.  
 
Master writes to slave RV-3032-C7 at specific address: 
 
1) Master sends out the START condition. 
2) Master sends out Slave Address, A2h for the RV-3032-C7; the 
WR/  bit is a 0 indicating a write operation. 
3) Acknowledgement from RV-3032-C7. 
4) Master sends out the Register Address to RV-3032-C7. 
5) Acknowledgement from RV-3032-C7. 
6) Master sends out the Data to write to the specified address in step 4). 
7) Acknowledgement from RV-3032-C7. 
8) Steps 6) and 7) can be repeated if necessary. 
  The address is automatically incremented in the RV-3032-C7. 
9) Master sends out the STOP Condition. 
 
S 0 PA A AREGISTER ADDRESSSLAVE ADDRESS DATA
Acknowledge from RV-3032-C7
R / W
ADATA
2 3 4 5 61 7 8 9
```

## Page 125

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 125/154 Rev. 1.3 
6.8. READ OPERATION AT SPECIFIC ADDRESS 
Master reads data from slave RV-3032-C7 at specific address: 
 
1) Master sends out the START condition. 
2) Master sends out Slave Address, A2h for the RV-3032-C7; the 
WR/  bit is a 0 indicating a write operation. 
3) Acknowledgement from RV-3032-C7. 
4) Master sends out the Register Address to RV-3032-C7. 
5) Acknowledgement from RV-3032-C7. 
6) Master sends out the Repeated START condition (or STOP condition followed by START condition) 
7) Master sends out Slave Address, A3h for the RV-3032-C7; the 
WR/  bit is a 1 indicating a read operation. 
8) Acknowledgement from RV-3032-C7. 
  At this point, the Master becomes a Receiver and the Slave becomes the Transmitter.  
9) The Slave sends out the Data from the Register Address specified in step 4). 
10) Acknowledgement from Master. 
11) Steps 9) and 10) can be repeated if necessary. 
  The address is automatically incremented in the RV-3032-C7. 
12) The Master, addressed as Receiver, can stop data transmission by generating a not-acknowledge on the 
  last byte that has been sent from the Slave-Transmitter. In this event, the Slave-Transmitter must leave the 
  data line HIGH to enable the Master to generate a STOP condition. 
13) Master sends out the STOP condition. 
 
PADATA DATAS 0 A SASLAVE ADDRESS SLAVE ADDRESS 1
R / W
R / W
REGISTER ADDRESS A
Acknowledge from MasterAcknowledge from R V-3032-C7
2 3 4 5 61 7 8 9 10 11 12 13
Not-acknowledge from Master
A
Repeated 
STAR T
 
 
 
 
6.9. READ OPERATION 
Master reads data from slave RV-3032-C7 immediately after first byte: 
 
1) Master sends out the START condition. 
2) Master sends out Slave Address, A3h for the RV-3032-C7; the 
WR/  bit is a 1 indicating a read operation. 
3) Acknowledgement from RV-3032-C7. 
  At this point, the Master becomes a Receiver and the Slave becomes the Transmitter. 
4) The RV-3032-C7sends out the Data from the last accessed Register Address incremented by 1.  
5) Acknowledgement from Master. 
6) Steps 4) and 5) can be repeated if necessary. 
  The address is automatically incremented in the RV-3032-C7. 
7) The Master, addressed as Receiver, can stop data transmission by generating a not-acknowledge on the 
  last byte that has been sent from the Slave-Transmitter. In this event, the Slave-Transmitter must leave the 
  data line HIGH to enable the Master to generate a STOP condition. 
8) Master sends out the STOP condition. 
 
S 1 PA ADATASLAVE ADDRESS DATA
Acknowledge from Master
R / W
Acknowledge from RV-3032-C7
2 3 4 5 61 7 8
A
Not-acknowledge from  Master
```

## Page 126

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 126/154 Rev. 1.3 
6.10. I2C-BUS IN SWITCHOVER CONDITION 
To save power when the RV-3032-C7 is in VBACKUP Power state the bus I2C-bus interface is automatically disabled 
(high impedance) and reset. Therefore the communication via  I2C interface should be terminated before the supply 
is switched from V DD to VBACKUP. If the bus communication could not be completed properly, the I 2C read/write data 
integrity is no longer guaranteed. 
If the I 2C communication has ended in an uncontrolle d manner, the I 2C-bus interface has to be re -initialized by 
sending a STOP followed by a START after the device switched back from VBACKUP Power state to VDD Power 
state. 
 
Note, that a debounce logic provides a debounce time t DEB which will filter VDD oscillation when switchover function 
will switch back from VBACKUP to VDD. I2C access is again possible in VDD Power state (and if VDD ≥ 1.4 V) after the 
debounce time tDEB. 
? tDEB MAX = 1 ms, when internal voltage was always above V LOW (typical 1.2 V). VLF = 0. 
? tDEB MAX = 1000 ms, when internal voltage was between V LOW (typical 1.2 V) and V POR (maximum 1.05 V). 
VLF = 1. See also BACKUP AND RECOVERY AC ELECTRICAL CHARACTERISTICS.
```

## Page 127

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 127/154 Rev. 1.3 
7. ELECTRICAL SPECIFICATIONS 
7.1. ABSOLUTE MAXIMUM RATINGS 
The following Table lists the absolute maximum ratings. 
 
Absolute Maximum Ratings according to IEC 60134: 
SYMBOL PARAMETER CONDITIONS MIN TYP MAX UNIT 
VDD Power Supply Voltage  -0.3  6.0 V 
VI Input voltage Input Pin -0.3  VDD +0.3 V 
VO Output voltage Output Pin -0.3  VDD +0.3 V 
II Input current  -10  10 mA 
IO Output current  -10  10 mA 
VESD ESD Voltage HBM (1)   ±2000 V 
CDM (2)   ±500 V 
ILU Latch-up Current Jedec (3)   ±100 mA 
TOPR Operating Temperature  -40  +85   (4) °C 
TSTO Storage Temperature  -55  +125 °C 
TPEAK Maximum reflow condition JEDEC J-STD-020C   +265 °C 
(1) HBM: Human Body Model, according to JEDEC JS-001. 
(2) CDM: Charged-Device Model, according to JEDEC JS-002. 
(3) Latch-up testing, according to JESD78, Class I (room temperature), level A (100 mA). 
(4) Supports extended operating temperature range from +85°C to +105°C with limitations.
```

## Page 128

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 128/154 Rev. 1.3 
7.2. OPERATING PARAMETERS 
For this Table, TA = -40 to +85°C unless otherwise indicated. VDD = 1.4 to 5.5 V, TYP values at 25°C and 3.0 V. 
 
Operating Parameters: 
SYMBOL PARAMETER CONDITIONS MIN TYP MAX UNIT 
Supplies 
VDD Power Supply Voltage 
TOPR = -40 to +105°C 
Timekeeping mode (1) 1.3  5.5 
V 
Minimum timekeeping 
voltage (2) 1.0  1.3 
Oscillator start-up voltage VSTART 1.3   
I2C-bus (100 kHz) 1.4  5.5 
I2C-bus (400 kHz) 2.0  5.5 
VBACKUP Backup Supply Voltage 
TOPR = -40 to +105°C 
Timekeeping mode (1) 1.3  5.5 
V Minimum timekeeping 
voltage (2) 1.0  1.3 
VLOW Voltage low detection 
(VLF flag) (3) 
Internal voltage (VDD or VBACKUP), 
(1 Hz sampling) 1.1 1.2 1.3 V 
IDD 
VDD supply current timekeeping 
I2C-bus inactive, CLKOUT 
disabled, average current (4) 
VDD = 1.3 V, TA = 25°C  160 210 
nA 
VDD = 3.0 V, TA = 25°C  160 210 
VDD = 5.0 V, TA = 25°C  165 220 
VDD = 1.3 V, -40 to +85°C   700 
VDD = 3.0 V, -40 to +85°C   750 
VDD = 5.0 V, -40 to +85°C   900 
IDD:EXT 
VDD supply current timekeeping 
I2C-bus inactive, CLKOUT 
disabled, average current (4) 
for extended temperature range 
VDD = 1.3 V, +85 to +105°C   1300 
nA VDD = 3.0 V, +85 to +105°C   1400 
VDD = 5.0 V, +85 to +105°C   1800 
IDD:I2C 
VDD supply current during 
I2C burst read/write, CLKOUT 
disabled (5) 
VDD = 1.4 V, SCL = 100 kHz  2 15 
µA VDD = 3.0 V, SCL = 400 kHz  5 40 
VDD = 5.0 V, SCL = 400 kHz  7 60 
ITSP Supply current temperature 
sensing peak (IDD or IBACKUP) Typical duration: tTSP = 1.3 ms  14 60 µA 
IDD:DSM 
VDD supply current in Direct 
Switching Mode, I2C-bus 
inactive, CLKOUT disabled 
VDD = 3.0 V, TA = 25°C, 
VBACKUP < VDD  165 260 
nA IDD:LSM 
VDD supply current in Level 
Switching Mode, I2C-bus 
inactive, CLKOUT disabled 
VDD = 3.0 V, TA = 25°C  190 300 
IBACKUP:DSM VBACKUP supply current in Direct 
Switching Mode 
VBACKUP = 3.0 V, TA = 25°C, 
VDD < VBACKUP  165 260 
IBACKUP:LSM VBACKUP supply current in Level 
Switching Mode 
VBACKUP = 3.0 V, TA = 25°C, 
VDD < VTH:LSM (2.0 V)  170 270 
Î”IDD:CK32 
Additional VDD supply current (6) 
VDD = 3.0 V, FCLKOUT = 32.768 
kHz, CL = 10 pF  1  µA 
Î”IDD:CK1024 VDD = 3.0 V, FCLKOUT = 1024 Hz, 
CL = 10 pF  30  
nA Î”IDD:CK64 VDD = 3.0 V, FCLKOUT = 64 Hz, 
CL = 10 pF  2  
Î”IDD:CK1 VDD = 3.0 V, FCLKOUT = 1 Hz, 
CL = 10 pF  0.03  
(1) Fully operating. 
(2) Clocks operating, RAM registers retained and VLOW sampling working, but temperature sensing and compensation is stopped, CLKOUT is 
  LOW and the I2C interface is disabled (VDD < 1.4 V). 
(3) VLF flag indicates a voltage drop below VLOW (typical 1.2 V). Data may no longer be valid and all registers should be reinitialized. 
(4) All inputs and outputs are at 0 V or VDD. 
(5) 2.2 kΩ pull-up resistors on SCL/SDA, excluding external peripherals and pull-up resistor current. All other inputs (except SDA and SCL) 
  are at 0 V or VDD. Test conditions: Continuous burst read/write, 55h data pattern, 25 Î¼s between each data byte, 20 pF load on each bus 
  pin. 
(6) When CLKOUT is enabled the additional VDD supply current Î”IDD can be calculated as follows: 
  Î”IDD = CL x VDD x fOUT, e.g. Î”IDD = 10 pF x 3.0 V x 32’768 Hz = 980 nA ≈ 1 µA
```

## Page 129

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 129/154 Rev. 1.3 
For this Table, TA = -40 to +85°C unless otherwise indicated. VDD = 1.4 to 5.5 V, TYP values at 25°C and 3.0 V. 
 
Operating Parameters (continued): 
SYMBOL PARAMETER CONDITIONS MIN TYP MAX UNIT 
Inputs 
VIH HIGH level input voltage VDD = 1.4 V to 5.5 V 
Pins: SCL, SDA, EVI 
0.8 VDD   V VIL LOW level input voltage   0.2 VDD 
IILEAK Input leakage current VSS ≤ VI ≤ VDD 
TA = -40°C to +105°C   ±0.5 µA 
CI Input capacitance VDD = 3.0 V, TA = 25°C 
f = 1 MHz   7 pF 
Outputs 
VOH:CLK 
HIGH level output voltage 
CLKOUT ≤ 32.768 kHz, 
CLMAX = 15 pF 
VDD = 1.4 V to < 2.7 V, 
IOH = -0.1 mA 0.9 VDD   
V 
VDD ≥ 2.7 V, IOH = -1.0 mA 
VOL:CLK 
LOW level output voltage, 
CLKOUT ≤ 32.768 kHz, 
CLMAX = 15 pF 
VDD = 1.4 V to < 2.7 V, 
IOL = 0.1 mA   0.1 VDD 
VDD ≥ 2.7 V, IOL = 1.0 mA 
VOH:CLK 
HIGH level output voltage 
CLKOUT > 32.768 kHz to 
52 MHz, CLMAX = 10 pF 
VDD = 2.7 V to 5.5 V, 
IOH = -2.0 mA 0.9 VDD   
VOL:CLK 
LOW level output voltage, 
CLKOUT > 32.768 kHz to 
52 MHz, CLMAX = 10 pF 
VDD = 2.7 V to 5.5 V, 
IOL = 2.0 mA   0.1 VDD 
tr, tf  
CLKOUT rise/fall time 
0.1 VDD to 0.9 VDD 
VDD = 2.7 V to 5.5 V 
FCLKOUT ≤ 32.768 kHz, 
CL = 15 pF  60 100 
ns 32.768 kHz < FCLKOUT ≤ 52 MHz, 
CL = 10 pF  - - 
VOL LOW level output voltage 
Pins: SDA, INTÌ…Ì…Ì…Ì…Ì… 
VDD = 1.4 V, IOL = 2.0 mA   0.4 
V VDD = 2.0 V, IOL = 3.0 mA   0.4 
VDD = 5.0 V, IOL = 3.0 mA   0.3 
IOLEAK Output leakage current 
Pins: SDA, INTÌ…Ì…Ì…Ì…Ì… 
VSS ≤ VO ≤ VDD 
TA = -40°C to +105°C   ±0.5 µA 
COUT Output capacitance VDD = 3.0 V, TA = 25°C 
f = 1 MHz   7 pF 
CL CLKOUT load capacitance 
(without COUT) 
FCLKOUT ≤ 32.768 kHz   15 pF 32.768 kHz < FCLKOUT ≤ 52 MHz   10 
Î´CLKOUT CLKOUT duty cycle VDD ≥ 2.7 V, FCLKOUT ≤ 52 MHz 50 ±10 % 
Power On Reset 
VPOR Falling edge threshold voltage PORF flag is cleared 
(and reset of all RAM data) 0.9 0.95 1.0 V 
Rising edge threshold voltage PROF flag is set (7) 0.95 1.0 1.05 V 
VHYS:VPOR VPOR detection hysteresis Rising edge - falling edge  50  mV 
Trickle Charger with Charge Pump 
TCM 1.75 V Regulated voltage 
(CeraCharge™ mode) VDD > VTH:LSM (maximum 2.2 V), 
LSM Mode (BSM = 10) 
1.6 1.75 1.9 
V TCM 3.0 V Charge pump voltage 2.75 3.0 3.25 
TCM 4.5 V 4.1 4.5 4.9 
TCR 0.6 kΩ 
Current limiting resistor VDD = 5.0 V, VBACKUP = 3.0 V, 
including internal schottky diode 
0.4 0.6 0.8 
kΩ TCR 2 kΩ 1.6 1.9 2.2 
TCR 7 kΩ 5.6 6.9 8.6 
TCR 12 kΩ 9.5 11.8 13.3 
VF Schottky diode voltage drop   0.25  V 
Switchover (see also BACKUP AND RECOVERY AC ELECTRICAL CHARACTERISTICS) 
VHYS:DSM Switchover hysteresis in Direct 
Switching Mode 
VDD with respect to VBACKUP ≥ 1.5 
V, VDD slew rate = ±1 V/ms 
TOPR = -40 to +85°C 
50 60 130 mV 
VTH:LSM Backup switchover threshold 
voltage in Level Switching Mode VDD falling below VTH:LSM 1.8 2.0 2.2 V 
VHYS:LSM Switchover hysteresis in Level 
Switching Mode 
VDD with respect to VBACKUP = 3.0 
V, VDD slew rate = ±1 V/ms 
TOPR = -40 to +85°C 
80 100 200 mV 
(7) If PORF is set to 1, it can be assumed that some or all RAM registers have been reset (internal voltage was below 0.9 V). 
  Data no longer valid.
```

## Page 130

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 130/154 Rev. 1.3 
For this Table, TA = -40 to +85°C unless otherwise indicated. VDD = 1.4 to 5.5 V, TYP values at 25°C and 3.0 V. 
 
Operating Parameters (continued): 
SYMBOL PARAMETER CONDITIONS MIN TYP MAX UNIT 
EEPROM Characteristics 
VDD:READ 
VDD read voltage 
(POR refresh and Automatic 
refresh) 
VDD Power state 
1.3   
V VDD read voltage 
(Refresh and Read one 
EEPROM byte) (I2C used) 
1.4   
VDD:WRITE VDD write voltage 1.6   
VDD:EEF EEPROM write access failure 
detection (EEF flag)    1.5 V 
tPREFR POR refresh time (1) At power up  ~66  
ms 
tAREFR Automatic refresh time (1) Each 24 hours, EERD = 0  ~1.4  
tUPDATE Update time (1) EECMD = 11h  46  
tREFR Refresh time (1) EECMD = 12h  1.4  
tWRITE Write to one EEPROM byte 
time (1) EECMD = 21h 1.2 4.8 9 
tREAD Read one EEPROM byte time (1) EECMD = 22h  1.1  
nCYCLE Write cycle endurance (2) 
VDD = 3.0 V, TA = 25°C 10’000   cycles VDD = 5.5 V, TA = 85°C 100   
tRET Data retention time (2) TA = 55°C 10   years 
(1) Time until EEbusy status bit is reset to 0. 
(2) Guaranteed by indirect testing. 
 
 
7.2.1. TEMPERATURE COMPENSATION AND CURRENT CONSUMPTION 
Typical average current IAVER (IDD or IBACKUP): 
 
ITSP
0 nA
IAVER
ITSP_OFF
Compensation interval (1 second)
1 Hz tick
tTSPtTSP_OFF
 
 
IAVER = ((ITSP_OFF × tTSP_OFF) + (ITSP × tTSP)) / 1 s 
 
IAVER = ((ITSP_OFF × (1 s - tTSP)) + (ITSP × tTSP)) / 1 s 
 
IAVER = ((145 nA × 998.7 ms ) + (14 µA × 1.3 ms)) / 1 s = 163 nA
```

## Page 131

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 131/154 Rev. 1.3 
7.2.2. TYPICAL CHARACTERISTICS 
Typical characteristics for Direct Switching Mode (DSM), Level Switching Mode (LSM) and switchover disabled: 
For these diagrams, I2C-bus inactive, CLKOUT disabled. 
 
 
IDD versus VDD, VBACKUP = 0 V, TA = 25°C 
 
0 1 2 3 4 5
VDD [V]
0
IDD [nA]
6
400
200
100
300
500
600
* 
switchover LSM
switchover disabled
DSM or
 
 
 
IBACKUP versus VBACKUP, VDD = 0 V, TA = 25°C 
(after turning off VDD) 
0 1 2 3 4 5
VBACKUP [V]
0
IBACKUP [nA]
6
400
200
100
300
500
600
DSM or LSM
switchover disabled
 
 
 
 
 
IDD versus TA, VDD = 3 V, VBACKUP = 0 V 
 
-50 -25 0 25 50 75
TA [°C]
0
IDD [nA]
100
800
400
200
600
1000
1200
125
LSM
DSM or switchover disabled
 
 
 
IBACKUP versus TA, VBACKUP = 3 V, VDD = 0 V 
(after turning off VDD) 
-50 -25 0 25 50 75
TA [°C]
0
IBACKUP [nA]
100
800
400
200
600
1000
1200
125
switchover disabled
DSM or LSM
```

## Page 132

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 132/154 Rev. 1.3 
7.3. XTAL OSCILLATOR PARAMETERS 
For this Table, TA = -40 to +85°C unless otherwise indicated. VDD = 1.4 to 5.5 V, TYP values at 25°C and 3.0 V. 
 
Oscillator Parameters: 
SYMBOL PARAMETER CONDITIONS MIN TYP MAX UNIT 
Xtal General 
F Crystal Frequency   32.768  kHz 
tSTART Oscillator start-up time at 
VDD = 3.0 V 
TA = 25°C  0.1 0.5 s TA = -40 to +105°C   3 
VSTART Oscillator start-up voltage TA = -40 to +105°C 1.3   V 
Î”f/V Frequency vs. voltage 
characteristics 
VDD = 1.5 V to 5.5 V 
TA = 25°C  0.5 1 ppm/V 
VDDR VDD rising slew rate 
(Clock maintenance slew rate) 
VDD = 1.1 V to 3.6 V   5 
V/µs VDD = 3.6 V to 5.5 V   15 
VDDF VDD falling slew rate 
(Clock maintenance slew rate) VDD = 5.5 V to 1.1 V   2 
Î´CLKOUT CLKOUT duty cycle VDD = 1.1 V to 5.5 V 
FCLKOUT = 32.768 kHz 50 ±10 % 
Xtal Frequency Characteristics 
Î”F/F Frequency accuracy TA = 25°C   ±50 ppm 
Î”F/FTOPR Frequency vs. temperature 
characteristics 
TOPR = -40 to +105°C 
VDD = 3.0 V -0.035ppm/°C2 (TOPR-T0)2  ±10% ppm 
T0 Turnover temperature  +25 ±5 °C 
Î”F/F Aging first year max. TA = 25°C, VDD = 3.0 V   ±3 ppm 
Digital Temperature Compensated Xtal DTCXO 
Î”t/t 
Time accuracy calibrated, 
OFFSET = 0 (default value on 
delivery). 
CLKOUT measured on rising 
edge of One 1 Hz period 
TA = -40°C to +85°C  ±1 ±2.5 ppm 
 ±0.09 ±0.22 s/day 
TA = +85°C to +105°C   ±20 ppm 
  ±1.73 s/day 
Î”t/t 
1 Hz OFFSET value: 
Min. corr. step (LSB) and 
Max. corr. range 
TA = -40°C to +105°C ±0.2384  +7.391/ 
-7.629 ppm 
Î”t/t OFFSET. Achievable time 
accuracy. 
Calibrated at an initial 
temperature and voltage -0.1192  +0.1192 ppm 
Î”T 
Temperature sensor accuracy 
calibrated, 
TREF = preconfigured (Factory 
Calibrated) 
TA = -40°C to +85°C  ±1 ±3 
°C TA = +85°C to +105°C   ±7 
Î”T TREF value: 
Min. corr. step (LSB) TA = -40°C to +105°C ±0.0078125 °C 
Î”T/V Temperature sensor value vs. 
voltage characteristics 
VDD = 1.5 V to 5.5 V 
TA = 25°C  0.1  °C/V
```

## Page 133

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 133/154 Rev. 1.3 
7.3.1. TIME ACCURACY VS. TEMPERATURE CHARACTERISTICS 
 
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
-225
-50 -30 -10 10 30 50 70 90 110
25
-40 -20 0 20 40 60 80 100
T0 = 25°C ±5°C
Tuning Fork Crystal
TA [°C]
Xtal 32.768 kHz
Time accuracy
Î”F/F = -0.035*(TA-T0)2 ppm ±10%
```

## Page 134

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 134/154 Rev. 1.3 
7.3.2. TIME ACCURACY 1 HZ EXAMPLES 
The following curves are with  OFFSET = 0 ( default value on delivery ). Fine adjustment in the vertical direction can 
be done by determining an OFFSET value (see AGING CORRECTION). 
 
Time accuracy of the temperature compensated 1 Hz signal at CLKOUT pin, T A = -40°C to +85°C: 
 
-4
-3
-2
-1
0
1
2
3
4
-50 -40 -30 -20 -10 0 10 20 30 40 50 60 70 80 90 TA [°C]
Î”t/t [ppm]
 
 
 
 
Time accuracy of the temperature compensated 1 Hz signal at CLKOUT pin, including extended range from 
+85°C to +105°C: 
 
Î”t/t [ppm]
-50 -40 -30 -20 -10 0 10 20 30 40 50 60 70 80 90 100 110 TA [°C]
-25
15
25
20
0
10
5
-15
-5
-10
-20
```

## Page 135

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 135/154 Rev. 1.3 
7.3.3. TEMPERATURE SENSOR ACCURACY EXAMPLE 
The following curves are made with preconfigured (Factory Calibrated) TREF value. Fine adjustment in the vertical 
direction can be done by determining a new TREF value (see TEMPERATURE REFERENCE ADJUSTMENT. 
For this diagram, RTC module is in VDD Power state so that the TEMP value can be read with I2C. 
 
Calibrated Temperature sensor accuracy, TA = -40°C to +85°C: 
 
-4
-3
-2
-1
0
1
2
3
4
-50 -40 -30 -20 -10 0 10 20 30 40 50 60 70 80 90 TA [°C]
Î”T [°C]
 
 
 
 
Calibrated Temperature sensor accuracy, including extended range from +85°C to +105°C: 
 
Î”T [°C]
-50 -40 -30 -20 -10 0 10 20 30 40 50 60 70 80 90 100 110 TA [°C]
-8
4
8
6
0
2
-4
-2
-6
```

## Page 136

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 136/154 Rev. 1.3 
7.4. POWER ON AC ELECTRICAL CHARACTERISTICS 
The following Figure describes the power on AC electrical characteristics for the CLKOUT pin. The clock output signal 
on CLKOUT pin is primarily controlled by the NCLKE bit (EEPROM C0h), and OS bit and the FD and HFD fields 
(EEPROM C1h). See also PROGRAMMABLE CLOCK OUTPUT and POWER ON RESET INTERRUPT FUNCTION. 
 
Power On AC Electrical Characteristics: Example with Power On Reset Interrupt and CLKOUT enabled. 
 
 
tDELAY
tPOR
Hi-Z
PORF
VDD
State POR Operating
I2C access disabled enabled
Write operation
CLKOUT
VPOR
tSTART
tPREFR
1
VDDR1
3
4
INT
5 62
 
 
1
 CLKOUT is set to LOW, after power on reset tPOR = typical 6 ms where CLKOUT is high-impedance (Hi-Z). 
2
 Flag PORF is set because VDD was below VPOR (Power On Reset event detected). 
3
 If the PORIE bit (EEPROM C1h) was set to 1 beforehand (in EEPROM), the PORIE bit in the RAM  mirror is 
  set to 1 after the typical start-up time tSTART = 0.1 s including the first refreshment time tPREFR = ~66 ms and 
  the INTÌ…Ì…Ì…Ì…Ì… pin output goes LOW. 
4
 Depending of the settings of the NCLKE bit (EEPROM C0h), and OS bit and the FD and HFD fields  
  (EEPROM C1h) the CLKOUT pin can drive the following signals: 
? Square wave of 32.768 kHz (default value on delivery), 1024 Hz, 64 Hz or 1 Hz, or 
an HFD frequency (8.192 kHz to 67.109 MHz in 8.192 kHz steps). 
? When NCLKE bit is 1 the CLKOUT signal is set to LOW level (if CLKF = 0). 
  CLKOUT is enabled after a typical delay time t DELAY, which depends on the selected frequency (synchronized 
  enable). See the Power On AC Electrical Parameters on the next page. 
5
 The PORF flag remains 1 until it is cleared to 0 by software. 
6
 If the INTÌ…Ì…Ì…Ì…Ì… pin is LOW, its status changes as soon as the PORF flag is cleared to 0.
```

## Page 137

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 137/154 Rev. 1.3 
For this Table, TA = -40 to +105°C and VDD = 1.3 to 5.5 V, TYP values at 25°C and 3.0 V. 
 
Power On AC Electrical Parameters: 
SYMBOL PARAMETER CONDITIONS MIN TYP MAX UNIT 
VDDR1 VDD rising slew rate at initial 
power on reset (POR)  10  55 V/ms 
tPOR Power on reset   6 10 ms 
tSTART Oscillator start-up time at 
VDD = 3.0 V        (1) 
TA = 25°C  0.1 0.5 s    3 
VSTART Oscillator start-up voltage  1.3   V 
tPREFR First refreshment time   66  ms 
tDELAY CLKOUT enable delay time 
(synchronized enable) 
FD = 32768 Hz  0.95  
ms 
FD = 1024 Hz  1.28  
FD = 64 Hz  6.65  
FD = 1 Hz  999  
HFD = 1.007616 MHz 
(example)  2.8  
(1)  If VDD ≥ 1.4 V, software can check I2C Acknowledge to get the shortest time I2C interface is active after Power On. 
  Or, when enabled, the POR interrupt can be used to do this.
```

## Page 138

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 138/154 Rev. 1.3 
7.5. BACKUP AND RECOVERY AC ELECTRICAL CHARACTERISTICS 
As long as no voltage drop of the internal voltage (VDD or V BACKUP) under V LOW (maximum 1.3  V) is detected the 
module is in timekeeping mode and fully operating. 
If the internal voltage falls below VLOW but is still in minimum timekeeping range (minimum 1.05 V) (VPOR MAX), clocks 
are operating, RAM registers are retained  and VLOW sampling working, but temperature sensing and compensation 
is stopped, CLKOUT is LOW and the I 2C interface (V DD < 1.4 V) is disabled. Data may no longer be valid and all 
registers should be reinitialized. 
 
1. If you want to use the CLKOUT function, select a valid V DD range (1.3 V < VDD ≤ 5.5 V). See also 
VOLTAGE LOW DIAGRAM. 
2. Ensure that the slew rates VDDF and VDDR2 fulfill their specifications. 
3. Check if these required specifications are fulfilled on your system. 
 
VDD Backup and recovery AC Electrical Characteristics: Example with Direct Switching Mode. 
 
VDD
0 V
VBACKUP
Write operation
VDD PowerPower state VBACKUP Power VDD Power
enabledI2C access disabled enabled
enable
switchover
VDDR2VDDF
tDEBtSWA
 
 
 
 
For this Table, TA = -40°C to +105°C unless otherwise indicated. 
 
VDD Backup and recovery AC Electrical Parameters: 
SYMBOL PARAMETER CONDITIONS MIN TYP MAX UNIT 
tSWA Backup switchover function 
activation time 
Changing from SWITCHOVER 
DISABLED to DSM   2 
ms Changing from SWITCHOVER 
DISABLED to LSM   10 
VDDF VDD falling slew rate    550 V/µs 
VDDR2 VDD rising slew rate Rising from 1.5 V to VDD   400 V/µs 
tDEB 
Debounce time from VDD 
debounce logic, when switching 
back from VBACKUP to VDD 
Internal voltage was always 
above VLOW (typical 1.2 V). 
VLF = 0. 
  1 
ms Internal voltage was between 
VLOW (typical 1.2 V) and VPOR 
(maximum 1.05 V). 
VLF = 1.         (1) 
  1000 
(1)  When VLF flag was set, the debounce time tDEB can take up to 1 second because 1 Hz sampling is used to detect if internal voltage is 
  back above VLOW (typical 1.2 V). If VDD ≥ 1.4 V, software can check I2C Acknowledge to get the shortest time I2C interface is active again.
```

## Page 139

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 139/154 Rev. 1.3 
7.6. I2C-BUS CHARACTERISTICS 
The following Figure and Table describe the I2C AC electrical parameters. 
 
I2C AC Parameter Definitions: 
 
t BUF
SCL
SDA
t HD:STA
t LOW
t RISE
SDA t SU:STA
t HD:DAT
t HIGH
t SU:DAT
t SU:STO
t FALL
SP
Sr P
 
 
 
For the following Table, TA = -40 to +85°C. 
 
I2C AC Electrical Parameters: 
SYMBOL PARAMETER VDD ≥ 1.4 V VDD ≥ 2.0 V UNIT MIN MAX MIN MAX 
fSCL SCL input clock frequency 0 100 0 400 kHz 
tLOW Low period of SCL clock 4.7  1.3  µs 
tHIGH High period of SCL clock 4.0  0.6  µs 
tRISE Rise time of SDA and SCL  1000  300 ns 
tFALL Fall time of SDA and SCL  300  300 ns 
tHD:STA START condition hold time 4.0  0.6  µs 
tSU:STA START condition setup time 4.7  0.6  µs 
tSU:DAT SDA setup time 250  100  ns 
tHD:DAT SDA hold time 0  0  µs 
tSU:STO STOP condition setup time 4.0  0.6  µs 
tBUF Bus free time before a new transmission 4.7  1.3  µs 
S = Start condition, Sr = Repeated Start condition, P = Stop condition 
 
Caution: 
When accessing the RV-3032-C7, all communication from transmitting the Start condition to transmitting the  Stop 
condition after access should be completed within 950 ms. 
If such communication requires 950 ms or longer, the I2C-bus interface is reset by the internal bus timeout function.
```

## Page 140

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 140/154 Rev. 1.3 
8. TYPICAL APPLICATION CIRCUITS 
8.1. NO BACKUP SOURCE / EVENT INPUT NOT USED 
Application Key Points: 
 
 
 
 
Register Configuration: 
 
RV-3032-C7 MCU
VDD VDD
VSSVSS
SCL
SDA
SCL
SDA
100 nF
4
CLKOUT
6
EVI
INT
5
VDD
1
2
VBACKUP
10 k  
10 k  3
 
 
1
 Backup Switchover functionality is disabled by default (default value on delivery). Do not leave VBACKUP 
   power supply pin floating. Connection to VSS through a 10 kΩ resistor keeps functional test possible. 
2
 100 nF decoupling capacitor close to the device. 
3
 Interrupts are disabled by default. PORIE and VLIE are disabled (default values on delivery). 
   INTÌ…Ì…Ì…Ì…Ì… pin is an open-drain output, which can be left open when not used. 
4
 I2C lines SCL, SDA are open-drain and require pull-up resistors to VDD. 
5
 CLKOUT with a frequency of 32.768 kHz (1) is enabled by default (default value on delivery). 
   If not used, disable CLKOUT to minimize current consumption (NCLKE = 1 and CLKF = 0). 
6
 External Events functionality is always active in VDD Power state. Do not leave EVI input pin floating. 
   Connection to VSS through a 10 kΩ resistor keeps functional test possible. 
   Note that in this example (EVI to VSS) the EVF flag will be set at POR. 
● No VBACKUP source 
● Lowest current consumption (160 nA typ.) 
● CLKOUT settings stored in EEPROM for permanent configuration 
0. RTC with default configuration on delivery (bits in black) 
1. Register C0h 0 1 0 0 0 0 0 0 NCLKE → CLKOUT disabled 
Register C2h X X X X X X X X HFD → High freq. to be selected 
Register C3h 
OS FD HFD[12:8] OS → Oscillator to be selected 
X X X X X X X X FD → Low freq. to be selected 
HFD → High freq. to be selected 
2. CLKOUT settings (C0h, C2h and C3h) to be stored in EEPROM using procedure of 4.6.3. 
(1) CLKOUT offers the selectable frequencies 32.768 kHz (default value on delivery), 1024 Hz, 64 Hz or 1 Hz, 
  or a frequency between 8192 Hz to 67.109 MHz in 8192 Hz steps for application use.
```

## Page 141

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 141/154 Rev. 1.3 
8.2. NON-RECHARGEABLE BACKUP SOURCE / EVENT INPUT USED (ACTIVE HIGH) 
Application Key Points: 
 
 
 
 
 
Register Configuration: 
 
RV-3032-C7 MCU
VDD VDD
VSSVSS
SCL
SDA
SCL
SDA
100 nF
2
CLKOUT 5
EVI
INT
INT
4
VDD
Primary
Battery
1 2
3
VBACKUP
100 nF
VBACKUP
VBACKUP
10 k  
6
 
 
1
 Insert a protection resistor of 100 - 1000 Î© to prevent damage in case of soldering issues causing short 
   between supply pins. 
2
 100 nF decoupling ceramic capacitor close to the device for VDD and VBACKUP. 
3
 The INTÌ…Ì…Ì…Ì…Ì… signal also works when the device operates on V BACKUP supply voltage. 
   In that case, it is possible to tie the INTÌ…Ì…Ì…Ì…Ì… signal pull-up resistor to VBACKUP. 
4
 I2C lines SCL, SDA are open-drain and require pull-up resistors to VDD. 
5
 CLKOUT is disabled in VBACKUP Power state. If not used in VDD Power state, disable CLKOUT to 
   minimize current consumption (NCLKE = 1 and CLKF = 0). 
6
 EVI input set to detect rising edge or high-level of tamper detection signal; 
   The EVI input is never floating thanks to the 10 kΩ to V SS. 
 
● Trickle charger disabled to avoid dangerous charging current into the backup source  
● LSM Backup Switchover Mode to avoid non-desired backup switching (VTH:LSM = 2.0 V) 
● Power Management settings have to be stored in EEPROM for permanent configuration  
● Rising edge or high-level voltage applied to the EVI input triggers an interrupt 
0. RTC with default configuration on delivery (bits in black) 
1. Register 11h 0 0 0 0 0 1 0 0 EIE → Event interrupt enabled 
Register 15h  EHL ET     EHL → Event-high detection 
0 1 X X 0 0 0 0 ET → Event filtering to be set 
Register C0h  NCLKE BSM TCR TCM BSM → LSM switchover mode 
0 X 1 0 0 0 0 0 TCM → Trickle charger disabled 
2. Switchover and CLKOUT settings (C0h) to be stored in EEPROM using procedure of 4.6.3.
```

## Page 142

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 142/154 Rev. 1.3 
8.3. RECHARGEABLE BACKUP SOURCE / EVENT INPUT USED (ACTIVE LOW) 
Application Key Points: 
 
 
 
 
 
Register Configuration: 
 
RV-3032-C7
VDD
VSS
SCL
SDA
CLKOUT
EVI
INT
Supercap or 
Rechargeable 
Battery
1
2
VBACKUP
100 nF
VBACKUP
6
5
MCU
VDD
VSS
SCL
SDA
INT
4
VDD
3
VBACKUP
100 nF
2
10 k  
 
1
 Low-cost MLCC (1) ceramic capacitor, supercapacitor (e.g. 1 farad) or secondary battery LMR 
   (respect manufacturer specifications for constant charging voltage). 
   When Lithium Battery is used, it is recommended to insert a protection resistor of 100 - 1000 Î©. to limit 
   battery current and to prevent damage in case of soldering issues causing short between supply pins.  
2
 100 nF decoupling ceramic capacitor close to the device for VDD and VBACKUP. 
3
 The INTÌ…Ì…Ì…Ì…Ì… signal also works when the device operates on VBACKUP supply voltage. 
   In that case, it is possible to tie the INTÌ…Ì…Ì…Ì…Ì… signal pull-up resistor to VBACKUP. 
4
 I2C lines SCL, SDA are open-drain and require pull-up resistors to VDD. 
5
 CLKOUT is disabled in VBACKUP Power state. If not used in VDD Power state, disable CLKOUT to 
   minimize current consumption (NCLKE = 1 and CLKF = 0). 
6
 EVI input set to detect falling edge or low-level of tamper detection signal; 
   The EVI input is never floating thanks to the 10 kΩ to V DD. 
 
● MLCC, Supercap or Rechargeable Battery as secondary VBACKUP source  
● DSM Backup Switchover Mode for capacitors (or LSM for rechargeable battery)  
● Backup source charged through the trickle charger with charge pump 
● Power Management settings have to be stored in EEPROM for permanent configuration 
0. RTC with default configuration on delivery (bits in black) 
1. Register 11h 0 0 0 0 0 1 0 0 EIE → Event interrupt enabled 
Register 15h  EHL ET     EHL → Event-low detection 
0 0 X X 0 0 0 0 ET → Event filtering to be set 
Register C0h 
 NCLKE BSM TCR TCM BSM → DSM switchover mode 
0 X 0 1 X X X X TCR → Trickle resistor to be set 
TCM → Trickle mode to be set 
2. Switchover and CLKOUT settings (C0h) to be stored in EEPROM using procedure of 4.6.3. 
(1) Note, that low-cost MLCCs are normally used for short timekeeping (minutes) and the more 
  expensive supercapacitors for a longer backup time (days - weeks).
```

## Page 143

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 143/154 Rev. 1.3 
8.4. CERACHARGE™ BACKUP BATTERY / EVENT INPUT NOT USED 
Application Key Points: 
 
 
 
 
 
Register Configuration: 
 
RV-3032-C7
VDD
VSS
100 nF
2
VBACKUP
SCL
SDA
CLKOUT 5
INT
3
6
2
100 nF
MCU
VDD
VSS
4
VDD
SCL
SDA
EVI
10 k  
TDK s
CeraCharge  
1
 
 
1
 First charging of the TDK’s CeraCharge™ (1) must be applied after soldering. Polarity will be applied with 
   the first charging. Maximum current: < 200 µA (End current: < 10 µA). Calculated maximum current: 
   1.75 V / 12 kÎ© = 146 µA. Settings: BSM = LSM Mode, TCR = 12 kÎ©, TCM = 1.75 V. 
2
 100 nF decoupling ceramic capacitor close to the device for VDD and VBACKUP. 
3
 Interrupts are disabled by default. PORIE and VLIE are disabled (default values on delivery).  
   INTÌ…Ì…Ì…Ì…Ì… pin is an open-drain output, which can be left open when not used. 
4
 I2C lines SCL, SDA are open-drain and require pull-up resistors to VDD. 
5
 CLKOUT is disabled in VBACKUP Power state. If not used in VDD Power state, disable CLKOUT to 
   minimize current consumption (NCLKE = 1 and CLKF = 0). 
6
 External Events functionality is always active in VDD Power state. Do not leave EVI input pin floating. 
   Connection to VSS through a 10 kΩ resistor keeps functional test possible. 
   Note that in this example (EVI to VSS) the EVF flag will be set at POR. 
 
● TDK’s CeraCharge™ - Rechargeable solid-state SMD battery as secondary VBACKUP source 
● LSM Backup Switchover Mode (VTH:LSM = 2.0 V). LSM also required for TCM 1.75V to be selected. 
● CeraCharge™ charged with constant voltage (CV) through the trickle charger with charge pump 
● Power Management settings have to be stored in EEPROM for permanent configuration  
0. RTC with default configuration on delivery (bits in black) 
1. 
Register C0h 
 NCLKE BSM TCR TCM BSM → LSM switchover mode 
0 X 1 0 1 1 0 1 TCR → 12 kΩ series resistor 
TCM → 1.75 V regulated voltage 
2. Switchover and CLKOUT settings (C0h) to be stored in EEPROM using procedure of 4.6.3. 
(1) Note, that the CeraCharge™ is normally used for longer backup time (days - weeks). 
  See also https://www.tdk-electronics.tdk.com/en/ceracharge.
```

## Page 144

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 144/154 Rev. 1.3 
8.5. NO BACKUP SOURCE / EVENT INPUT USED (“WAKE-UP” & “POWER SWITCH”) 
Application Key Points: 
 
 
 
 
 
Register Configuration: 
 
RV-3032-C7
MCU
VDD VDD
VSSVSS
SCL
SDA
SCL
SDA
CLKOUT
5
EVI
INT INT
43
VBACKUP
Power Switch
Wake up !
Hold !
GPIO
VDD
LO
HI
Application 
Wake up !
6
8
1
10 k  
100 nF
2
7 User
Wake up !
10 k  
VIN VOUT
ON GND
VIN VOUT
ONGND
 
1
 Backup Switchover functionality is disabled by default. Do not leave VBACKUP power supply pin floating. 
   Connection to VSS through a 10 kΩ resistor keeps functional test possible. 
2
 100 nF decoupling capacitor close to the device. 
3
 INTÌ…Ì…Ì…Ì…Ì… pin is an open-drain output and requires a pull-up resistor. 
4
 I2C lines SCL, SDA are open-drain and require pull-up resistors to VDD. 
5
 Disable CLKOUT to minimize current consumption (NCLKE = 1 and CLKF = 0). 
6
 EVI input set to detect rising edge or high-level of tamper detection signal; can be used as an Application 
   Wake-Up signal. The EVI Input is never floating thanks to the 10 kΩ to V SS. 
7
 User or Manual Wake-Up, always available; e.g for initial system power-on to configure RTC and system. 
8
 MCU Power Retention via GPIO = High maintains MCU Power to complete I 2C Interface communication 
   with the RTC. MCU cuts-off it’s own supply voltage by set GPIO = Low at the very end of its task. 
 
● No VBACKUP source and lowest current consumption (160 nA typ.) 
● External Event enabled allowing RTC to fire “wake-up” interrupt acting on load switch 
● MCU most of the time in idle mode is awaken by RTC’s interrupt through the upper load switch  
● MCU holds supply voltage until its task is finished and cuts off its own supply voltage 
0. RTC with default configuration on delivery (bits in black) 
1. Register 10h 0 0 0 0 0 1 0 0 EIE → Event interrupt enabled 
Register 15h  EHL ET     EHL → Event-high detection 
0 1 X X 0 0 0 0 ET → Event filtering to be set
```

## Page 145

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 145/154 Rev. 1.3 
9. PACKAGE 
9.1. DIMENSIONS AND SOLDER PAD LAYOUT 
 
RV-3032-C7 Package: 
 
      Package dimensions (bottom view):       Recommended solder pad layout: 
    
0,4 3,2
0,4 0,8
2,0
1,5
0,9
0,8
0,9
0,50,15
3,2 0,90,9
0,5
1
5
3
7
42
68
0,8 max  
                          Metal lid is connected to VSS (pin #5) 
                           
                          Dimensions:  in mm 
                          Tolerances:  unless otherwise specified ±0.1mm 
                          Drawing:    RV-3032-C7_Pack-drw_20200615 
 
 
 
9.1.1. RECOMMENDED THERMAL RELIEF 
When connecting a pad to a copper plane, thermal relief is recommended. 
 
 
RV-C7 Package: 
GOOD
P
BAD
O
```

## Page 146

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 146/154 Rev. 1.3 
9.2. MARKING AND PIN #1 INDEX 
 
Laser marking RV-3032-C7 Package: (top view) 
 
#1 #4
#5#8
M214A1
3032
Pin 1 Index
Part Designation
Product Date Code
M 2 1 4 A 1
M
Micro Crystal
2
Year
14
Week
A1
Batch
```

## Page 147

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 147/154 Rev. 1.3 
10. MATERIAL COMPOSITION DECLARATION & ENVIRONMENTAL INFORMATION 
 
10.1. HOMOGENOUS MATERIAL COMPOSITION DECLARATION 
Homogenous material information according to IPC-1752 standard 
 
Material Composition RV-3032-C7: 
4
5
2
1
8
3
6
7
 
 
 
No. Item 
Component 
Name 
Sub Item 
Material 
Name 
Material 
Weight 
Substance 
Element 
CAS 
Number 
Comment 
(mg) (%) 
1 Resonator Quartz Crystal 0.13 100% SiO2  14808-60-7  
2 Electrodes Cr+Au 0.01 6% Cr Cr: 7440-47-3  94% Au Au: 7440-57-5 
3 Housing Ceramic 6.90 100% Al2O3  1344-28-1  
4 Metal Lid Kovar Lid 
2.67 
95% Fe53Ni29Co18 Fe:
Ni: 
Co: 
7439-89-6 
7440-02-0 
7440-48-4 
Metal Lid (Kovar) 
Ni-plating 4.95% Ni Ni: 7440-02-0 Nickel plating 
Au-plating 0.05% Au Au: 7440-57-5 Gold plating 
5 Seal Solder Preform 0.54 80% Au80 / Sn20 Au: 7440-57-5  20% Sn: 7440-31-5 
6 Terminations Internal and external 
terminals 0.38 
80% Mo Mo: 7439-98-7 Molybdenum 
15% Ni Ni: 7440-02-0 Nickel plating 
5% Au  0.5 micron Au: 7440-57-5 Gold plating 
7 Conductive 
adhesive 
Silver filled Silicone 
glue 
0.09 
88% Ag Ag: 7440-22-4  
12% Siloxanes and 
silicones  68083-19-2 di-Me, vinyl group-
terminated 
0% Distillates, petroleum 
hydrotreated  64742-47-8 Does not appear in 
finished product 
8 CMOS IC Silicon 0.64 90% Si Si: 7440-21-3  
Gold bumps 10% Au Au: 7440-57-5  
  Unit weight 11.4      
 
  
(Symbolic drawing)
```

## Page 148

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 148/154 Rev. 1.3 
10.2. MATERIAL ANALYSIS & TEST RESULTS 
Homogenous material information according to IPC-1752 standard 
No. Item 
Component 
Name 
Sub Item 
Material 
Name 
RoHS Halogens Phthalates 
Pb 
Cd 
Hg 
Cr(VI) 
PBB 
PBDE 
F 
CI 
Br 
I 
BBP 
DBP 
DEHP 
DIBP 
1 Resonator Quartz Crystal nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
2 Electrodes Cr+Au nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
3 Housing Ceramic nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
4 Metal Lid Kovar Lid & Plating nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
5 Seal Solder Preform nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
6 Terminations Int. & ext.  terminals nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
7 Conductive 
adhesive Silver filled Silicone glue nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
8 CMOS IC Silicon & Gold bumps nd nd nd nd nd nd nd nd nd nd nd nd nd nd 
 MDL [ppm] Method Detection Limit 2 8 5 50 50 
                             nd (not detected) = below “Method Detection Limit” (MDL) 
Test methods: 
RoHS        Test method with reference to: 
? Pb, Cd       IEC 62321-5:2013                 MDL:  2 ppm 
? Hg         IEC 62321-4:2013 + AMD1:2017         MDL:  2 ppm 
? Cr(VI)       IEC 62321-7-2:2017                MDL:  8 ppm 
? PBB / PBDE    IEC 62321-6:2015                 MDL:  5 ppm 
Halogens      Test method with reference to BS EN 14582:2016   MDL:  50 ppm  
Phthalates      Test method with reference to IEC 62321-8:2017   MDL:  50 ppm
```

## Page 149

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 149/154 Rev. 1.3 
10.3. RECYCLING MATERIAL INFORMATION 
Recycling material information according to IPC-1752 standard. 
Element weight is accumulated and referenced to the unit weight of 11.4 mg. 
Item 
Material 
Name 
No. Item 
Component 
Name 
Material 
Weight 
Substance 
Element 
CAS 
Number 
Comment 
(mg) (%) 
Quartz Crystal 1 Resonator 0.13 1.14 SiO2  14808-60-7  
Chromium 2 Electrodes 0.0006 0.005 Cr Cr: 7440-47-3  
Ceramic 3 Housing 6.90 60.74 Al2O3  1344-28-1  
Gold 2 
4 
5 
6 
8 
Electrodes 
Metal Lid 
Seal 
Terminations 
CMOS IC 
0.53 4.63 Au Au: 7440-57-5  
Tin 5 Seal 0.11 0.95 Sn Sn: 7440-31-5  
Nickel 4 
6 
Metal Lid 
Terminations 0.19 1.67 Ni Ni: 7440-02-0  
Molybdenum 6 Terminations 0.3 2.68 Mo Mo: 7439-98-7  
Kovar 4 Metal Lid 
2.53 22.33 Fe53Ni29Co18 
Fe:
Ni: 
Co: 
7439-89-6 
7440-02-0 
7440-48-4 
 
Silver 7a Conductive adhesive 0.079 0.7 Ag Ag: 7440-22-4  
Siloxanes and 
silicones 
7b Conductive adhesive 0.011 0.10 Siloxanes and 
silicones  68083-19-2 di-Me, vinyl group-
terminated 
Distillates 7c Conductive adhesive 
0 0 Distillates  64742-47-8 
hydrotreated 
petroleum, does not 
appear in finished 
products 
Silicon 8 CMOS IC 0.58 5.07 Si Si: 7440-21-3  
 Unit weight (total) 11.4 100
```

## Page 150

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 150/154 Rev. 1.3 
10.4. ENVIRONMENTAL PROPERTIES & ABSOLUTE MAXIMUM RATINGS 
 
Package Description 
SON-8 ceramic package Small Outline Non-leaded (SON), hermetically sealed ceramic package with metal lid 
 
Parameter Directive Conditions Value 
Product weight (total)   11.4 mg 
Storage temperature  Store as bare product -55 to +125°C 
Moisture sensitivity level (MSL) IPC/JEDEC J-STD-020D  MSL1 
FIT / MTBF    available on request 
 
 
Terminal finish: 
 
Ceramic
Molybdenum
Nickel
Gold
0.5 µm
5 µm
```

## Page 151

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 151/154 Rev. 1.3 
11. SOLDERING INFORMATION 
 
Maximum Reflow Conditions in accordance with IPC/JEDEC J-STD-020C “Pb-free” 
 
25
Time
Temperature
TP
TL
tP
tL
t  25°C to Peak
Ramp-up
ts
Preheat
Tsmin
Critical Zone
TL to TP
Tsmax
Ramp-down
 
 
Temperature Profile Symbol Condition Unit 
Average ramp-up rate   (Tsmax to TP) 3°C / second max °C / s 
Ramp down Rate Tcool 6°C / second max °C / s 
Time 25°C to Peak Temperature Tto-peak 8 minutes max min 
Preheat 
Temperature min Tsmin 150 °C 
Temperature max Tsmax 200 °C 
Time Tsmin  to Tsmax ts  60 - 180 sec 
Soldering above liquidus 
Temperature liquidus TL 217 °C 
Time above liquidus tL 60 - 150  sec 
Peak temperature 
Peak Temperature Tp 260 °C 
Time within 5°C of peak temperature tp 20 - 40 sec
```

## Page 152

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 152/154 Rev. 1.3 
12. HANDLING PRECAUTIONS FOR MODULES WITH EMBEDDED CRYSTALS 
The built-in tuning-fork crystal consists of pure Silicon Dioxide in crystalline form. The cavity inside the package is 
evacuated and hermetically sealed in order for the crystal blank to function undisturbed from air molecules, humidity 
and other influences. 
 
Shock and vibration: 
 
Keep the crystal / module from being exposed to excessive mechanical shock and vibration . Micro Crystal 
guarantees that the crystal / module will bear a mechanical shock of 5000 g / 0.3 ms.  
 
The following special situations may generate either shock or vibration: 
 
Multiple PCB panels - Usually at the end of the pick & place process the single PCBs are cut out with a router. 
These machines sometimes generate vibrations on the PCB that have a fundamental or harmonic frequency close 
to 32.768 kHz. This might cause breakage of crystal blanks due to resonance. Router speed should be adjusted 
to avoid resonant vibration. 
 
Ultrasonic cleaning - Avoid cleaning processes using ultrasonic energy. These processes can damage the 
crystals due to the mechanical resonance frequencies of the crystal blank. 
 
Overheating, rework high temperature exposure: 
 
Avoid overheating the package. The package is sealed with a seal ring consisting of 80% Gold and 20% Tin. The 
eutectic melting temperature of this alloy is at 280°C. Heating the seal ring up to >280°C will cause melting of the 
metal seal which then, due to the vacuum, is sucked into the cavity forming an air duct. This happens when using 
hot-air-gun set at temperatures >280°C. 
 
Use the following methods for rework: 
 
? Use a hot-air-gun set at 270°C. 
? Use 2 temperature controlled soldering irons , set at 270°C, with special-tips to contact all solder-joints from 
both sides of the package at the same time, remove part with tweezers when pad solder is liquid.
```

## Page 153

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 153/154 Rev. 1.3 
13. PACKING & SHIPPING INFORMATION 
 
Reel: 7” = 178 mm 
max. 17 
min. 12.4 Ø 13 
 
 
 
 
Carrier Tape: 
Material: Polycarbonate, conductive 
Width: 12 mm 
Tape Leader and Trailer: Minimum length 300 mm 
2 ±0,05
4 ±0,1
3,43 ±0,05
4 ±0,1
12
5,5 ±0,05
1,75 ±0,1
0,254 ±0,015
1,73 ±0,05 0,83 ±0,05(0,06)
Cover Tape
0,314 ±0,05
+0,3
 -0,1
All dimensions are in mm
RV-3032-C7_Tape-drw_20221104
Direction of feed
3032
3032
 
Cover Tape: 
Tape: Polypropylene, 3M™ Universal Cover Tape (UCT) 
Adhesive Type: Pressure sensitive, Synthetic Polymer 
Thickness: 0.06 mm 
 
Peel Method: 
Medial section removal, both lateral stripes remain on carrier
```

## Page 154

```text
Micro Crystal 
 
DTCXO Temp. Compensated Real-Time Clock Module RV-3032-C7 
 
 
May 2023 154/154 Rev. 1.3 
14. COMPLIANCE INFORMATION 
Micro Crystal confirms that the standard product Real-Time Clock Module RV-3032-C7 is compliant with “EU RoHS 
Directive” and “EU REACh Directives”. 
Please find the actual Certificate of Conformance for Environmental Regulations on our website:  
CoC_Environment_RV-Series.pdf 
 
 
15. DOCUMENT REVISION HISTORY 
Date Revision # Revision Details 
June 2020 1.0 First release 
August 2020 1.1 Added Operating RV-3032-C7 With CeraCharge™ Backup Battery, 8.1.1. 
November 2022 1.2 
Clarified and corrected various specifications 
Adapted schemes, diagrams and application circuits 
Added extended temperature range specifications, +85°C to +105°C 
Adapted limit values and methods in accordance with the latest standards, 10.2. 
Added new disclaimer, 15. 
May 2023 1.3 
Corrected that EVI function is disabled in VBACKUP Power state, 1., 2.2., 2.3. 
3.22., 4.2., 4.7., 4.11., 4.17., 8.1. and 8.4. 
Added that EVI pin should not be tied to VBACKUP, 2.2. 
Adjusted TCM 3.0 V and 4.5 V values, 3.20.1, 4.3. and 7.2. 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
The information contained in this document is believed to be accurate and reliable. However, Micro Crystal 
assumes no responsibility for any consequences resulting from the use of such information nor for any 
infringement of patents or other rights of third parties, which may result from its use. In accordance with our 
policy of continuous development and improvement, Micro Crystal reserves the right to mod ify 
specifications mentioned in this publication without prior notice and as deemed necessary.  
 
Any use of Products for the manufacture of arms is prohibited. Customer shall impose that same obligation 
upon all third-party purchasers. 
Without the express w ritten approval of Micro Crystal, Products are not authorized for use as components 
in safety and life supporting systems as well as in any implantable medical devices. The unauthorized use 
of Products in such systems / applications / equipment is solely a t the risk of the customer and such 
customer agrees to defend and hold Micro Crystal harmless from and against any and all claims, suits, 
damages, cost, and expenses resulting from any unauthorized use of Products.  
 
No licenses to patents or other intellec tual property rights of Micro Crystal are granted in connection with 
the sale of Micro Crystal products, neither expressly nor implicitly. In respect of the intended use of Micro 
Crystal products by customer, customer is solely responsible for observing ex isting patents and other 
intellectual property rights of third parties and for obtaining, as the case may be, the necessary licenses.  
 
 
 
 
Micro Crystal AG 
Muehlestrasse 14 
CH-2540 Grenchen 
Switzerland 
Phone +41 32 655 82 82 
sales@microcrystal.com 
www.microcrystal.com
```
