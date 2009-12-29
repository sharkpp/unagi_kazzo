#include <stdint.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h> 
#include <avr/wdt.h>
#include "bus_access.h"
/*
firmware bootloader sequence
- User insert a cartrige which has charcter RAM. eg. UNROM or SGROM
- Host send new firmware image with REQUEST_PPU_WRITE.
- Host receive and compare charcter memory image.
- Firmware reprogram by REQUEST_FIRMWARE_PROGRAM.

compiler notice/ WinAVR 20080610
compiler produces bad object with optimize option -O2
use -Os
*/
__attribute__((noreturn))
BOOTLOADER_SECTION
void mcu_data_program(uint8_t *buf, const uint16_t bufsize, uint16_t address, uint16_t length)
{
	uint16_t offset = 0;
	int page;
	cli();
	wdt_disable();

	eeprom_busy_wait();
	for(page = 0; page < length / SPM_PAGESIZE; page++){
		int i;
		mcu_programdata_read(offset, SPM_PAGESIZE, buf);
		boot_page_erase(address);
		boot_spm_busy_wait();
		for(i = 0; i < SPM_PAGESIZE; i += 2){
			//Set up little-endian word
			uint16_t w = buf[i+0];
			w |= buf[i+1] << 8;
			boot_page_fill(i, w);
		}
		boot_page_write(address);
		boot_spm_busy_wait();
		offset += SPM_PAGESIZE;
		address += SPM_PAGESIZE;
	}
	boot_rww_enable();
	sei();
	wdt_enable(WDTO_500MS);
endless: //wait watchdog interruptting reset
	goto endless;
}
