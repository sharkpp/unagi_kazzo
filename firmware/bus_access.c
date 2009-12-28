#include <util/delay.h>
#include <avr/io.h>
#include "type.h"
#include "bus_access.h"

//avr/io.h use many macros, I want use IO access by inline function...
#define IO_DIRECTION(NAME) (DDR##NAME)
#define IO_OUT(NAME) (PORT##NAME)
#define IO_IN(NAME) (PIN##NAME)
/* PAx: output only
connected CPU and PPU databus*/
#define ADDRESSBUS_A0_A7_DIR IO_DIRECTION(A)
#define ADDRESSBUS_A0_A7_OUT IO_OUT(A)
/* PBx: output/input
connected address high latch(HC574), CPU and PPU databus*/
#define DATABUS_DIR IO_DIRECTION(B)
#define DATABUS_OUT IO_OUT(B)
#define DATABUS_IN IO_IN(B)
enum databus_dir{
	DATABUS_DIR_OUT = 0xff,
	DATABUS_DIR_IN = 0
};
/*PCx: output ADDRESS_HIGH_LATCH connect HC574 clock pin, bus control signal
VRAM_CS is input port this is design mistake!
*/
#define BUS_CONTROL_DIR IO_DIRECTION(C)
#define BUS_CONTROL_OUT IO_OUT(C)
enum iobit_bus_control{
	CPU_PHI2 = 0, CPU_ROMCS, CPU_RW,
	RESERVE_PPU_POS_A13, PPU_RD, PPU_WR,
	VRAM_CS, ADDRESS_HIGH_LATCH
};
//when cpu_write_flash, phi2 must be low. when phi2 is high, mmc3 and vrc4 changes bank.
enum {
	BUS_CLOSE = 0xff
};
/*PDx: use input, empty pin is output*/
#define USB_MISC_DIR IO_DIRECTION(D)
#define USB_MISC_PULLUP IO_OUT(D)
#define USB_MISC_IN IO_IN(D)
enum iobit_usb_misc{
	USB_DPLUS = 2, CPU_IRQ, 
	USB_DMINUS, VRAM_A10
};
static inline uint8_t bit_get_negative(enum iobit_bus_control bit)
{
	uint8_t ret = (1 << bit);
	return ~ret;
}

void bus_init(void)
{
	ADDRESSBUS_A0_A7_DIR = 0xff;
	ADDRESSBUS_A0_A7_OUT = 0;
	DATABUS_DIR = DATABUS_DIR_OUT;
	BUS_CONTROL_DIR = bit_get_negative(VRAM_CS); //VRAM_CS is input port
	BUS_CONTROL_OUT = BUS_CLOSE;
	USB_MISC_DIR = (0b1100 << 4) | 0b0011; //empty pin use OUT
	USB_MISC_PULLUP = (1 << CPU_IRQ) | (1 << VRAM_A10);
}

/*
make phi2 edge signal, this is needed by namcot mapper and RP2C33.
*/
void phi2_init(void)
{
	int i = 0x80;
	while(i != 0){
		BUS_CONTROL_OUT = BUS_CLOSE;
		BUS_CONTROL_OUT = BUS_CLOSE ^ (1 << CPU_PHI2);
		i--;
	}
}

//for RAM adapter DRAM refresh
void phi2_update(void)
{
	static uint8_t i = 0;
	uint8_t c = BUS_CLOSE;
	if(i & 0b100){
		c ^= 1 << CPU_PHI2;
	}
	BUS_CONTROL_OUT = c;
	i += 1;
}
/*
address high databus assignment
D0-D5: CPU and PPU A8-A13
D6: CPU A14
D7: PPU /A13
*/
static void address_set(uint16_t address)
{
	ADDRESSBUS_A0_A7_OUT = address & 0xff;
	uint8_t high = (address & 0x7fff) >> 8; //mask A0-A14
	if((address & (1 << 13)) == 0){ //if A13 == 0
		high |= 0x80; //set /A13
	}
	DATABUS_OUT = high;
	BUS_CONTROL_OUT = bit_get_negative(ADDRESS_HIGH_LATCH);
	BUS_CONTROL_OUT = BUS_CLOSE;
}
static inline void direction_write(void)
{
	DATABUS_DIR = DATABUS_DIR_OUT;
	asm("nop");
	asm("nop");
	asm("nop");
}

static inline void direction_read(void)
{
	DATABUS_OUT = 0xff; //when input direction, pullup
	DATABUS_DIR = DATABUS_DIR_IN;
	asm("nop"); //wait for chaging port direction. do not remove.
	asm("nop");
	asm("nop");
}
//mmc5 ROM area need that phi2 is high
void cpu_read(uint16_t address, uint16_t length, uint8_t *data)
{
	BUS_CONTROL_OUT = BUS_CLOSE;
	while(length != 0){
//		uint8_t c = BUS_CLOSE;
		direction_write();
		address_set(address);
		if((address & 0x8000) != 0){
//			c &= bit_get_negative(CPU_ROMCS) | (1 << CPU_ROMCS);
//			BUS_CONTROL_OUT = c;
			BUS_CONTROL_OUT = bit_get_negative(CPU_ROMCS);
		}
		direction_read();
		*data = DATABUS_IN;
		data += 1;
		BUS_CONTROL_OUT = BUS_CLOSE;
		address += 1;
		length--;
	}
	direction_write();
}

void cpu_read_6502(uint16_t address, uint16_t length, uint8_t *data)
{
	while(length != 0){
		//phi2 down
		uint8_t c = bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = c;

		//down -> up
		direction_write();
		address_set(address);
		if((address & 0x8000) != 0){
			c &= bit_get_negative(CPU_ROMCS);
		}
		BUS_CONTROL_OUT = c;
		clock_wait(1);
		
		//phi2 up
		c |= (1 << CPU_PHI2); // | (1 << CPU_ROMCS);
		BUS_CONTROL_OUT = c;
		if(1){
			direction_read();
			*data = DATABUS_IN;
			data += 1;
		}
		clock_wait(1);
		if(0){
			BUS_CONTROL_OUT = c;
			direction_read();
			*data = DATABUS_IN;
			data += 1;
		}
		BUS_CONTROL_OUT = c;
		
		//phi2 down
		if((address & 0x8000) != 0){
			c &= bit_get_negative(CPU_ROMCS);
		}
		c &= bit_get_negative(CPU_PHI2);
		if(0){
			BUS_CONTROL_OUT = c;
			direction_read();
			*data = DATABUS_IN;
			data += 1;
		}
		clock_wait(1);
		
		//bus close
		BUS_CONTROL_OUT = BUS_CLOSE;
		
		address += 1;
		length--;
	}
	direction_write();
}

void ppu_read(uint16_t address, uint16_t length, uint8_t *data)
{
	//BUS_CONTROL_OUT = BUS_CLOSE;
	while(length != 0){
		direction_write();
		address_set(address);
		BUS_CONTROL_OUT = bit_get_negative(PPU_RD);
		direction_read();
		*data = DATABUS_IN;
		data += 1;
		BUS_CONTROL_OUT = BUS_CLOSE;
		address += 1;
		length--;
	}
	direction_write();
}

enum compare_status cpu_compare(uint16_t address, uint16_t length, const uint8_t *data)
{
	return OK;
}
enum compare_status ppu_compare(uint16_t address, uint16_t length, const uint8_t *data)
{
	while(length != 0){
		direction_write();
		address_set(address);
		BUS_CONTROL_OUT = bit_get_negative(PPU_RD);
		direction_read();
		if(DATABUS_IN != *data){
			BUS_CONTROL_OUT = BUS_CLOSE;
			direction_write();
			return NG;
		}
		data += 1;
		BUS_CONTROL_OUT = BUS_CLOSE;
		address += 1;
		length--;
	}
	direction_write();
	return OK;
}

void cpu_write_6502_nowait(uint16_t address, uint16_t length, const uint8_t *data)
{
	while(length != 0){
		uint8_t control;
		address_set(address);
		
		//phi2 down
		control = bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = control;
		control &= bit_get_negative(CPU_RW);
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
		}
		BUS_CONTROL_OUT = control;
		
		//phi2 up
		control |= (1 << CPU_PHI2) | (1 << CPU_ROMCS);
		BUS_CONTROL_OUT = control;
		DATABUS_OUT = *data;
		data++;
		
		//phi2 down
		control &= bit_get_negative(CPU_PHI2);
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
		}
		BUS_CONTROL_OUT = control;
		
		//bus close
		BUS_CONTROL_OUT = BUS_CLOSE;
		
		address += 1;
		length--;
	}
}

/*
/WE controlled write operation has busconflict
PHI2  |-__________-
R/W   |----___-----
/ROMCS|--_______---
A0-A14|-<vaild address>-
D0-D7 |--oo<i>**---
o is dataout, i is datain, * is bus-confilict

/CS controlled write operation is clean write cycle for flash memory
PHI2  |-__________-
R/W   |--_______---
/ROMCS|----___-----
A0-A14|-<vaild address>-
D0-D7 |----<iii>---
*/
static inline void cpu_write_flash_waveform(uint16_t address, uint8_t data)
{
	uint8_t control = bit_get_negative(CPU_PHI2);
	address_set(address);
	if(0){ //R/W = /WE controlled write operation
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
			BUS_CONTROL_OUT = control;
		}
		control &= bit_get_negative(CPU_RW);
		BUS_CONTROL_OUT = control;
		DATABUS_OUT = data;
		control |= 1 << CPU_RW; //R/W close
		BUS_CONTROL_OUT = control;
	}else{ ///ROMCS = /CS controlled write operation
		control &= bit_get_negative(CPU_RW);
		BUS_CONTROL_OUT = control;
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
			BUS_CONTROL_OUT = control;
		}
		DATABUS_OUT = data;
		control |= 1 << CPU_ROMCS;
		BUS_CONTROL_OUT = control;
	}
	BUS_CONTROL_OUT = BUS_CLOSE;
}
void cpu_write_flash(uint16_t address, uint16_t length, const uint8_t *data)
{
	direction_write();
	while(length != 0){
		cpu_write_flash_waveform(address, *data);
		data++;
		address += 1;
		length--;
	}
}

void cpu_write_flash_order(const struct flash_order *t)
{
	int length = FLASH_PROGRAM_ORDER;
	direction_write();
	while(length != 0){
		cpu_write_flash_waveform(t->address, t->data);
		t++;
		length--;
	}
}
/*
NTSC hardware timing
Master clock fsc: 21.4772272 MHz
CPU clock fsc/12: 1.789773MHz
clock per second 12/fsc: 5.58*10**-7 sec, 0.55 us
*/
void cpu_write_6502(uint16_t address, uint16_t length, const uint8_t *data)
{
	while(length != 0){
		uint8_t control;
		address_set(address);
		
		//phi2 down
		control = bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = control;
		control &= bit_get_negative(CPU_RW);
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
		}
		BUS_CONTROL_OUT = control;
		clock_wait(1);

		//phi2 up
		control |= (1 << CPU_PHI2); //| (1 << CPU_ROMCS);
		DATABUS_OUT = *data;
		BUS_CONTROL_OUT = control;
//		DATABUS_OUT = *data;
		data++;
		clock_wait(1);
		
		//phi2 down
		control &= bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = control;
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
		}
//		clock_wait(1);
		BUS_CONTROL_OUT = control;
		
		//bus close
//		clock_wait(1);
		BUS_CONTROL_OUT = BUS_CLOSE;
		
		address += 1;
		length--;
	}
}

static inline void ppu_write_waveform(uint16_t address, uint8_t data)
{
	address_set(address);//PPU charcter memory /CS open
	BUS_CONTROL_OUT = bit_get_negative(PPU_WR);
	DATABUS_OUT = data;
	BUS_CONTROL_OUT = BUS_CLOSE;
	address_set(0x3fff); ///CS close, use pallete area. When address bus is 0x2000-0x2fff, some cartriges enable tilemap area.
}
void ppu_write(uint16_t address, uint16_t length, const uint8_t *data)
{
	while(length != 0){
		ppu_write_waveform(address, *data);
		data++;
		address += 1;
		length--;
	}
}

void ppu_write_order(const struct flash_order *t)
{
	int length = FLASH_PROGRAM_ORDER;
	while(length != 0){
		ppu_write_waveform(t->address, t->data);
		t++;
		length--;
	}
}

uint8_t vram_connection_get(void)
{
	uint8_t ret;
	uint16_t address = 0x2000;
	direction_write();
	address_set(address);
	ret = bit_get(USB_MISC_IN, VRAM_A10);
	address += 1 << 10;

	address_set(address);
	ret |= bit_get(USB_MISC_IN, VRAM_A10) << 1;
	address += 1 << 10;
	
	address_set(address);
	ret |= bit_get(USB_MISC_IN, VRAM_A10) << 2;
	address += 1 << 10;

	address_set(address);
	ret |= bit_get(USB_MISC_IN, VRAM_A10) << 3;
	address += 1 << 10;
	
	return ret;
}

__attribute__ ((section(".bootloader.bus")))
static void boot_address_set(uint16_t address)
{
	ADDRESSBUS_A0_A7_OUT = address & 0xff;
	uint8_t high = (address & 0x7fff) >> 8; //mask A0-A14
	if((address & (1 << 13)) == 0){ //if A13 == 0
		high |= 0x80; //set /A13
	}
	DATABUS_OUT = high;
	BUS_CONTROL_OUT = bit_get_negative(ADDRESS_HIGH_LATCH);
	BUS_CONTROL_OUT = BUS_CLOSE;
}

__attribute__ ((section(".bootloader.bus")))
void mcu_programdata_read(uint16_t address, uint16_t length, uint8_t *data)
{
	//BUS_CONTROL_OUT = BUS_CLOSE;
	while(length != 0){
		direction_write();
		boot_address_set(address);
		BUS_CONTROL_OUT = bit_get_negative(PPU_RD);
		direction_read();
		*data = DATABUS_IN;
		data += 1;
		BUS_CONTROL_OUT = BUS_CLOSE;
		address += 1;
		length--;
	}
	direction_write();
}
