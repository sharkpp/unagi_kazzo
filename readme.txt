famicom cartridge bus simulator 'kazzo'
unagi development team / 2009.10.08
firmware version 0.1.3 / 2010.03.13

----features----
- USB-to-PC communication
- ROM cartridge read and write access
- programming access for flash memory cartridge
- composed of few, mostly off-the-shelf parts
- firmware is powered by V-USB
  http://www.obdev.at/products/vusb/index.html
- firmware and host software are open source, licenced by GPL v2

kazzo was named after the Japanese traditional fish '鰹'.

----files----
firmware/
  firmware source code and Makefile. In order to complile, WinAVR 
  environment is required.
hostecho/
  source codes for kazzo_test.exe
hostmodule/
  module source codes for kazzo from 'unagi'
windows_driver/
  device driver for Windows
kazzo_test.exe
  loop back test client binary for Windows
kazzo_mega16.hex kazzo_mega164p.hex kazzo_mega168.hex
  firmware hex file written in Intel-Hex Record format
kazzo_pcb_1.png
  schematics graphic file for PCB 1.x
  (note: U1 pin number is assigned as ATmega16 QFP.)
kazzo_pcb_2.png
  schematics graphic file for PCB 2.x
mcu16.txt mcu88.txt
  pin and fusebit assignments
readme.txt
  this file
usbrequest.txt
  directions on how to send USB request commands to kazzo.
COPYING
  GPL v2 licencing document

Host software 'unagi' is not included in this package. unagi's binary 
and source codes are available from the official project page.
http://unagi.sourceforge.jp/

----hardware designs----
Hardware designs has 2 revisions. Check each docments.
ATmega16 or ATmega164P based design / PCB 1.x -> mcu16.txt
ATmega168 or ATmega168P based design / PCB 2.x -> mcu88.txt

PCB 2.x is a design to reduce the produce cost. The transfer rate is 
slower than PCB 1.x. Because ATmega168 uses PCINT port for interrupt. 
ATmega168 does not have empty IO pins. It's difficult to expansions.

If you assemble kazzo by yourself, we recommend PCB 1.x. We will not stop 
supporting PCB 1.x.

