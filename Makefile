MCU_164P = MCU=atmega164p TARGET=kazzo_mega164p
MCU_16 = MCU=atmega16 TARGET=kazzo_mega16

all:
	(cd firmware;make -f firmware.mak $(MCU_164P))
	(cd firmware;make -f firmware.mak $(MCU_16))
	(cd hostecho;make)
clean:
	(cd firmware;make -f firmware.mak $(MCU_164P) clean)
	(cd firmware;make -f firmware.mak $(MCU_16) clean)
	(cd hostecho;make clean)
p4p:
	(cd firmware;make -f firmware.mak $(MCU_164P) program)
