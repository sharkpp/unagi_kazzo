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
- Firmware calc charcter RAM checksum, firmware send result.
- Firmware reprogram by REQUEST_MCU_DATA_PROGRAM.
*/
enum{
	PROGRAM_MEMORYSIZE = 0x1c00,
	PROGRAM_PAGESIZE = PROGRAM_MEMORYSIZE / SPM_PAGESIZE
};

BOOTLOADER_SECTION
uint8_t mcu_data_check(uint16_t checksum, uint8_t *buf, const uint16_t bufsize)
{
	uint16_t sum = 0, address = 0;
	for(address = 0; address < PROGRAM_MEMORYSIZE; address += bufsize){
		int i;
		uint8_t sum8 = 0;
		mcu_programdata_read(address, bufsize, buf);
		for(i = 0; i < bufsize; i += 4){
			sum8 += buf[i + 0];
			sum8 += buf[i + 1];
			sum8 += buf[i + 2];
			sum8 += buf[i + 3];
		}
		sum += (uint16_t) sum8;
	}
	return checksum == sum;
}

__attribute__((noreturn))
BOOTLOADER_SECTION
void mcu_data_program(uint8_t *buf, const uint16_t bufsize)
{
	int page;
	uint16_t address = 0;
	cli();
	eeprom_busy_wait();
	for(page = 0; page < PROGRAM_PAGESIZE; page++){
		int i;
		mcu_programdata_read(address, SPM_PAGESIZE, buf);
//		eeprom_busy_wait();
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
		address += SPM_PAGESIZE;
	}
	boot_rww_enable();
	sei();
	wdt_enable(WDTO_500MS);
endless: //wait watchdog interrupt reset
	goto endless;
}
