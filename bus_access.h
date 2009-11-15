#ifndef _BUS_ACCESS_H_
#define _BUS_ACCESS_H_
void bus_init(void);
void phi2_init(void);
void phi2_update(void);
void cpu_read(uint16_t address, uint16_t length, uint8_t *data);
void cpu_read_6502(uint16_t address, uint16_t length, uint8_t *data);
void ppu_read(uint16_t address, uint16_t length, uint8_t *data);
void cpu_write_6502(uint16_t address, uint16_t length, const uint8_t *data);
void cpu_write_flash(uint16_t address, uint16_t length, const uint8_t *data);
void ppu_write(uint16_t address, uint16_t length, const uint8_t *data);

enum compare_status{
	OK, NG
};
enum compare_status cpu_compare(uint16_t address, uint16_t length, const uint8_t *data);
enum compare_status ppu_compare(uint16_t address, uint16_t length, const uint8_t *data);
enum{
	FLASH_PROGRAM_ORDER = 3
};
struct flash_order{
	uint16_t address;
	uint8_t data;
};
void cpu_write_flash_order(const struct flash_order *t);
void ppu_write_order(const struct flash_order *t);
#include <util/delay.h>
static inline void clock_wait(double clock)
{
	_delay_us(clock * 0.55);
}
#endif
