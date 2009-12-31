#ifndef _MCU_PROGRAM_H_
#define _MCU_PROGRAM_H_
struct bootloader_assign{
	const char version[0x20];
	__attribute__((noreturn))
	void (*const programmer)(uint8_t *buf, uint16_t address, uint16_t length);
};
//__attribute__ ((section(".bootloader.version")))
const struct bootloader_assign BOOTLOADER_ASSIGN;
#endif
