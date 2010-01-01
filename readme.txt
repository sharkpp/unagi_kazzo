famicom cartridge bus simulator 'kazzo'
unagi development team / 2009.10.08
firmware version 0.1.1 / 2010.01.02

----features----
- USB communication to PC
- read and write access for ROM cartridge
- program access for flash memory cartridge
- composed of few parts
- firmware is powered by V-USB
  http://www.obdev.at/products/vusb/index.html
- firmware and host software are opensource, licenced by GPL v2

kazzo was named from Japanese traditional fish '鰹'.

----files----
firmware/
  firmware source code and Makefile. To complile, required WinAVR 
  enviroment.
hostecho/
  source codes for kazzo_test.exe
hostmodule/
  module source codes for kazzo from 'unagi'
windows_driver/
  device driver for Windows
kazzo_test.exe
  loop back test client binary for Windows
kazzo_mega16.hex kazzo_mega164p.hex
  firmware hex file written in Intel-Hex Record
kazzo_schematics.png
  schematics graphic data
  notice! U1 pin number is assigned ATmega16 QFP.
readme.txt
  this file
usbrequest.txt
  It is written how to request to kazzo.
COPYING
  GPL v2 licencing document

Host software 'unagi' is not included in this package. 'unagi's binary 
and source codes are available from official project page.
http://unagi.sourceforge.jp/

----AVR fusebit configuration----
ATmega164P: low byte 0xee, high byte 0xd9, extended byte 0xff
  CKDIV8:1 CKOUT:1 SUT:10 CKSEL:1110
  OCDEN:1 JTAGEN:1 SPIEN:0 WDTON:1 EESAVE:1 BOOTSZ:00 BOOTRST:1
  BODLEVEL:111
ATmega16:  low byte 0xae, high byte 0xc9
  BODLEVEL:1 BODEN:0 SUT:10 CKSEL:1110
  OCDEN:1 JTAGEN:1 SPIEN:0 CKOPT:0 EESAVE:1 BOOTSZ:00 BOOTRST:1

----list of parts-----
#    |name
-----+-------------------
U1   |ATmega164P or ATmega16
U2   |74HC574
CN1  |type B female USB socket
CN2  |3x2 pin header, 2.54 mm spacing
CN3  |30x2 pin cardedge connecter, 2.54 mm spacing
R1,R2|68 ohm register
R3   |1.5 kohm register
R4   |30 kohm register
D1,D2|3.6 V zener diode
X1   |16.0 MHz ceramic resonator
C1,C2|0.1uF ceramic capacitor
CP1  |10uF electric capacitor
JP1  |toggle switch
JP2  |push switch

----pin assignment----
See schematics for switch, register, diode and capacitor connection.

CN3: cartridge connector CN1: USB socket type B
        +-----+                +---+              U1: ATmega164P (DIP)
     GND| 1 31|+5V          +5V|1 4|GND               +--v--+
 CPU A11| 2 32|CPU PHI2      D-|2 3|D+              D0| 1 40|A0
 CPU A10| 3 33|CPU A12         +---+                D1| 2 39|A1
  CPU A9| 4 34|CPU A13   CN2: ISP conncetor         D2| 3 38|A2
  CPU A8| 5 35|CPU A14         +---+                D3| 4 37|A3
  CPU A7| 6 36|CPU D7      MISO|1 2|Vcc             D4| 5 36|A4
  CPU A6| 7 37|CPU D6       SCK|3 4|MOSI       MOSI/D5| 6 35|A5
  CPU A5| 8 38|CPU D5    Reset#|5 6|GND        MISO/D6| 7 34|A6
  CPU A4| 9 39|CPU D4          +---+            SCK/D7| 8 33|A7
  CPU A3|10 40|CPU D3     U2: 74HC574           Reset#| 9 32|+5V
  CPU A2|11 41|CPU D2         +--v--+              Vcc|10 31|GND
  CPU A1|12 42|CPU D1      GND| 1 20|Vcc           GND|11 30|+5V
  CPU A0|13 43|CPU D0       D0| 2 19|A8          XTAL1|12 29|AHL
 CPU R/W|14 44|CPU ROMCS#   D1| 3 18|A9          XTAL2|13 28|VRAM CS#
CPU IRQ#|15 45|SOUND IN     D2| 4 17|A10            NC|14 27|PPU WR#
     GND|16 46|SOUND OUT    D3| 5 16|A11            NC|15 26|PPU RD#
 PPU RD#|17 47|PPU WR#      D4| 6 15|A12        USB D+|16 25|NC
VRAM A10|18 48|VRAM CS#     D5| 7 14|A13      CPU IRQ#|17 24|CPU R/W
  PPU A6|19 49|PPU A13#     D6| 8 13|CPU A14    USB D-|18 23|CPU ROMCS#
  PPU A5|20 50|PPU A7       D7| 9 12|PPU A13# VRAM A10|19 22|CPU PHI2
  PPU A4|21 51|PPU A8      GND|10 11|AHL            NC|20 21|NC
  PPU A3|22 52|PPU A9         +-----+                 +-----+
  PPU A2|23 53|PPU A10   
  PPU A1|24 54|PPU A11   
  PPU A0|25 55|PPU A12   
  PPU D0|26 56|PPU A13   
  PPU D1|27 57|PPU D7    
  PPU D2|28 58|PPU D6    
  PPU D3|29 59|PPU D5    
     +5V|30 60|PPU D4    
        +-----+          

----notice----
- AHL is Address High Latch.
- NC is No Connection.
- # is negative logic signal.
- D0-D7 are databus.
-- shared by U1, U2, CN3(CPU and PPU).
-- D5-D7 are shared by ISP signal.
- A0-A13 are addressbus. 
-- A0-A7 are shared by U1, CN3(CPU and PPU)
-- A8-A13 are shared by U2, CN3(CPU and PPU)
- CPU A14 and PPU A13# are uniq address buses. 
- U1 can substitute ATmega16.
- SOUND IN and SOUND OUT has no connection.
- If you don't need power switch, short JP1.
- If you don't need reset switch, open JP2.
