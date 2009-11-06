#ifndef _FLASHMEMORY_H_
#define _FLASHMEMORY_H_
//cpu
uint8_t flash_cpu_status(void);
void flash_cpu_config(uint16_t c000x, uint16_t c2aaa, uint16_t c5555, uint16_t unit);
void flash_cpu_program(uint16_t address, uint16_t length, uint8_t *data);
void flash_cpu_erase(uint16_t address);
void flash_cpu_device_get(uint8_t d[2]);
uint8_t flash_ppu_status(void);
//ppu
void flash_ppu_config(uint16_t c000x, uint16_t c2aaa, uint16_t c5555, uint16_t unit);
void flash_ppu_program(uint16_t address, uint16_t length, uint8_t *data);
void flash_ppu_erase(uint16_t address);
void flash_ppu_device_get(uint8_t d[2]);
//task
void flash_process(void);
#endif
