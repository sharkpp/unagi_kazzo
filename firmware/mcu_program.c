#include <stdint.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h> 
#include <avr/wdt.h>
#include "kazzo_request.h"
#include "bus_access.h"
#include "mcu_program.h"

#if SPM_PAGESIZE > READ_PACKET_SIZE 
 #error READ_PACKET_SIZE is few capacity
#endif
/*
firmware bootloader sequence
- User insert a cartrige which has charcter RAM and CPU work RAM.
- Host send new firmware image with REQUEST_PPU_WRITE and REQUEST_CPU_WRITE_6502.
- Host receive and compare translated memory image.
- Firmware reprogram by REQUEST_FIRMWARE_PROGRAM.

MEGA164P-20[AP]U
ISP programming -> REQUEST_FIRMWARE_PROGRAM -> flash memory is programmed 2 pages only...
ISP programming -> power off->on -> REQUEST_FIRMWARE_PROGRAM -> flash memory is programmed all pages.
*/
__attribute__((noreturn))
__attribute__ ((section(".bootloader.programmer")))
static void mcu_data_program(uint8_t *buf, uint16_t address, uint16_t length)
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
	if(0){ //force re-turn power on. keep watchdog disable
		boot_rww_enable();
		sei();
		wdt_enable(WDTO_500MS);
	}
endless: //wait watchdog interruptting reset
	goto endless;
}

__attribute__ ((section(".bootloader.version")))
const struct bootloader_assign BOOTLOADER_ASSIGN = {
	.version = "kazzo loader 0.1.0",
	.programmer = mcu_data_program
};

