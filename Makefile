all:
	make -f firmware.mak MCU=atmega164p TARGET=kazzo_mega164p
	make -f firmware.mak MCU=atmega16 TARGET=kazzo_mega16
clean:
	make -f firmware.mak MCU=atmega164p TARGET=kazzo_mega164p clean
	make -f firmware.mak MCU=atmega16 TARGET=kazzo_mega16 clean
