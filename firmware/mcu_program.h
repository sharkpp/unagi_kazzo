#ifndef _MCU_PROGRAM_H_
#define _MCU_PROGRAM_H_
__attribute__((noreturn))
void mcu_data_program(uint8_t *buf, const uint16_t bufsize, uint16_t address, uint16_t length);
#endif
