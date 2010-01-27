#include <util/delay.h>
#include <avr/io.h>
#include "type.h"
#include "bus_access.h"

//avr/io.h use many macros, I want use IO access by inline function...
#define IO_DIRECTION(NAME) (DDR##NAME)
#define IO_OUT(NAME) (PORT##NAME)
#define IO_IN(NAME) (PIN##NAME)
/*
--------- assignment and functions for PCB revision 1.x --------
*/
#if PCB_REVISION == 1
/* PAx: output only
connected CPU and PPU databus*/
#define ADDRESSBUS_A0_A7_DIR IO_DIRECTION(A)
#define ADDRESSBUS_A0_A7_OUT IO_OUT(A)
/* PBx: output/input
connected address high latch(HC574), CPU and PPU databus*/
#define DATABUS_DIR IO_DIRECTION(B)
#define DATABUS_OUT IO_OUT(B)
#define DATABUS_IN IO_IN(B)
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
/*PDx: use input, empty pin is output*/
#define USB_MISC_DIR IO_DIRECTION(D)
#define USB_MISC_PULLUP IO_OUT(D)
#define USB_MISC_IN IO_IN(D)
enum iobit_usb_misc{
	USB_DPLUS = 2, CPU_IRQ, 
	USB_DMINUS, VRAM_A10
};
#endif
/*
--------- assignment and functions for PCB revision 2.x --------
*/
#if PCB_REVISION == 2
/* PBx: misc IO 
0:IO:USB D+ / PCINT0.vector.PCI0
1:IO:USB D-
2:I :VRAM A10
3:I :VRAM CS# / MOSI
4: O:Address Low Latch / MISO
5: O:Address High Latch / SCK 
*/
#define USB_MISC_DIR IO_DIRECTION(B)
#define USB_MISC_PULLUP IO_OUT(B)
#define USB_MISC_IN IO_IN(B)
enum iobit_usb_misc{
	USB_DPLUS = 0, USB_DMINUS,
	VRAM_A10, VRAM_CS,
	ADDRESS_LOW_LATCH, ADDRESS_HIGH_LATCH,
	XTAL1, XTAL2
};
/* PCx: output; memory control + etc 
0:I :CPU IRQ# / PCINT8.vector.PCI1
1: O:CPU PHI2
2: O:CPU ROMCS#
3: O:CPU R/W
4: O:PPU RD#
5: O:PPU WR#
6:I :MCU RESET
*/
#define BUS_CONTROL_DIR IO_DIRECTION(C)
#define BUS_CONTROL_OUT IO_OUT(C)
enum iobit_bus_control{
	CPU_IRQ = 0, CPU_PHI2, CPU_ROMCS, CPU_RW,
	PPU_RD, PPU_WR
};
/*PD: IO; databus */
#define DATABUS_DIR IO_DIRECTION(D)
#define DATABUS_OUT IO_OUT(D)
#define DATABUS_IN IO_IN(D)
#endif

enum databus_dir{
	DATABUS_DIR_OUT = 0xff,
	DATABUS_DIR_IN = 0
};
//when cpu_write_flash, phi2 must be low. when phi2 is high, mmc3 and vrc4 changes bank.
enum {
	BUS_CLOSE = ~(1 << CPU_PHI2),
	ADDRESS_CLOSE = 0x3fff //CPU and PPU are mapped internal registers, cartridge closes buses
};

static inline uint8_t bit_get_negative(enum iobit_bus_control bit)
{
	uint8_t ret = (1 << bit);
	return ~ret;
}

#if PCB_REVISION == 1
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
	//phi2 pulse is needed mmc1
	BUS_CONTROL_OUT = bit_get_negative(ADDRESS_HIGH_LATCH);
	BUS_CONTROL_OUT = BUS_CLOSE;
}

void bus_init(void)
{
	ADDRESSBUS_A0_A7_DIR = 0xff;
	DATABUS_DIR = DATABUS_DIR_OUT;
	BUS_CONTROL_DIR = bit_get_negative(VRAM_CS); //VRAM_CS is input port
	BUS_CONTROL_OUT = BUS_CLOSE;
	USB_MISC_DIR = (0b1100 << 4) | 0b0011; //empty pin use OUT
	USB_MISC_PULLUP = (1 << CPU_IRQ) | (1 << VRAM_A10);
	address_set(ADDRESS_CLOSE);
}
#endif
#if PCB_REVISION == 2
static void address_set(uint16_t address)
{
	const uint8_t portb = 0x05;
	DATABUS_OUT = address & 0xff;
	asm("cbi %0,%1" : :"M"(portb),"M"(ADDRESS_LOW_LATCH));
	asm("sbi %0,%1" : :"M"(portb),"M"(ADDRESS_LOW_LATCH));
	uint8_t high = (address & 0x7fff) >> 8; //mask A0-A14
	if((address & (1 << 13)) == 0){ //if A13 == 0
		high |= 0x80; //set /A13
	}
	DATABUS_OUT = high;
	asm("cbi %0,%1" : :"M"(portb),"M"(ADDRESS_HIGH_LATCH));
	asm("sbi %0,%1" : :"M"(portb),"M"(ADDRESS_HIGH_LATCH));
	//phi2 pulse is needed mmc1
	BUS_CONTROL_OUT = BUS_CLOSE | (1 << CPU_PHI2);
	BUS_CONTROL_OUT = BUS_CLOSE;
}
void bus_init(void)
{
	DATABUS_DIR = DATABUS_DIR_OUT;
	BUS_CONTROL_DIR = (1 << CPU_PHI2) | (1 << CPU_ROMCS) | (1 << CPU_RW) | (1 << PPU_RD) | (1 << PPU_WR);
	BUS_CONTROL_OUT = BUS_CLOSE;
	USB_MISC_DIR = (1 << ADDRESS_HIGH_LATCH) | (1 << ADDRESS_LOW_LATCH);
	USB_MISC_PULLUP = (1 << VRAM_A10)| (1 << VRAM_CS);
	address_set(ADDRESS_CLOSE);
}
#endif

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
		uint8_t c = BUS_CLOSE;
		direction_write();
		address_set(address);
		if((address & 0x8000) != 0){
			c &= bit_get_negative(CPU_ROMCS);
//			BUS_CONTROL_OUT = c;
//			BUS_CONTROL_OUT = bit_get_negative(CPU_ROMCS);
		}
		c |= 1 << CPU_PHI2;
		BUS_CONTROL_OUT = c;
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
/*		uint8_t c = bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = c;*/
		uint8_t c = BUS_CLOSE;
		//down -> up
		direction_write();
		address_set(address);
		BUS_CONTROL_OUT = c;
		clock_wait(1);
		
		//phi2 up
		c |= (1 << CPU_PHI2);
		if((address & 0x8000) != 0){
			c &= bit_get_negative(CPU_ROMCS);
		}
		BUS_CONTROL_OUT = c;
		direction_read();
		clock_wait(1);
		*data = DATABUS_IN;
		data += 1;

		BUS_CONTROL_OUT = c;
		
		//phi2 down, bus close
		BUS_CONTROL_OUT = BUS_CLOSE;
		
		address += 1;
		length--;
	}
	address_set(ADDRESS_CLOSE);
}

void ppu_read(uint16_t address, uint16_t length, uint8_t *data)
{
	//BUS_CONTROL_OUT = BUS_CLOSE;
	while(length != 0){
		direction_write();
		address_set(address);
		BUS_CONTROL_OUT = bit_get_negative(PPU_RD) & bit_get_negative(CPU_PHI2);
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
	while(length != 0){
		BUS_CONTROL_OUT = BUS_CLOSE;
		direction_write();
		address_set(address);
		BUS_CONTROL_OUT = bit_get_negative(CPU_ROMCS) | (1 << CPU_PHI2);
		direction_read();
		if(DATABUS_IN != *data){
			BUS_CONTROL_OUT = BUS_CLOSE;
			direction_write();
			return NG;
		}
		data += 1;
		address += 1;
		length--;
	}
	BUS_CONTROL_OUT = BUS_CLOSE;
	direction_write();
	return OK;
}
enum compare_status ppu_compare(uint16_t address, uint16_t length, const uint8_t *data)
{
	while(length != 0){
		direction_write();
		address_set(address);
		BUS_CONTROL_OUT = bit_get_negative(PPU_RD) & bit_get_negative(CPU_PHI2);;
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
		control = bit_get_negative(CPU_RW) & bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = control;
		
		//phi2 up
		control |= (1 << CPU_PHI2);
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
		}
		BUS_CONTROL_OUT = control;
		
		//data set
		DATABUS_OUT = *data;
		data++;
		
		//phi2 down
		control &= bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = control;
		
		//bus close
		BUS_CONTROL_OUT = BUS_CLOSE;
		
		address += 1;
		length--;
	}
	address_set(ADDRESS_CLOSE);
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
	address_set(ADDRESS_CLOSE);
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
	address_set(ADDRESS_CLOSE);
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
		control = bit_get_negative(CPU_RW) & bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = control;
		clock_wait(1);

		//phi2 up
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
		}

		control |= (1 << CPU_PHI2);
		BUS_CONTROL_OUT = control;
		//data set
		DATABUS_OUT = *data;
		data++;
		clock_wait(1);
		
		//phi2 down
		control &= bit_get_negative(CPU_PHI2);
		BUS_CONTROL_OUT = control;
		if((address & 0x8000) != 0){
			control &= bit_get_negative(CPU_ROMCS);
		}
		BUS_CONTROL_OUT = control;
		
		//bus close
		BUS_CONTROL_OUT = BUS_CLOSE;
		clock_wait(1);
		
		address += 1;
		length--;
	}
	address_set(ADDRESS_CLOSE);
}

static inline void ppu_write_waveform(uint16_t address, uint8_t data)
{
	address_set(address);//PPU charcter memory /CS open
	BUS_CONTROL_OUT = bit_get_negative(PPU_WR) & bit_get_negative(CPU_PHI2);
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
#if PCB_REVISION == 1
	ADDRESSBUS_A0_A7_OUT = address & 0xff;
	uint8_t high = (address & 0x7fff) >> 8; //mask A0-A14
	if((address & (1 << 13)) == 0){ //if A13 == 0
		high |= 0x80; //set /A13
	}
	DATABUS_OUT = high;
	BUS_CONTROL_OUT = bit_get_negative(ADDRESS_HIGH_LATCH) & bit_get_negative(CPU_PHI2);
	BUS_CONTROL_OUT = BUS_CLOSE;
#endif
#if PCB_REVISION == 2
	const uint8_t portb = 0x05;
	DATABUS_OUT = address & 0xff;
	asm("cbi %0,%1" : :"M"(portb),"M"(ADDRESS_LOW_LATCH));
	asm("sbi %0,%1" : :"M"(portb),"M"(ADDRESS_LOW_LATCH));
	uint8_t high = (address & 0x7fff) >> 8; //mask A0-A14
	if((address & (1 << 13)) == 0){ //if A13 == 0
		high |= 0x80; //set /A13
	}
	DATABUS_OUT = high;
	asm("cbi %0,%1" : :"M"(portb),"M"(ADDRESS_HIGH_LATCH));
	asm("sbi %0,%1" : :"M"(portb),"M"(ADDRESS_HIGH_LATCH));
#endif
}

__attribute__ ((section(".bootloader.bus")))
void mcu_programdata_read(uint16_t address, uint16_t length, uint8_t *data)
{
	while(length != 0){
		direction_write();
		if(address < 0x2000){ //PPU CHR-RAM
			boot_address_set(address);
			BUS_CONTROL_OUT = bit_get_negative(PPU_RD) & bit_get_negative(CPU_PHI2);
		}else{ //CPU W-RAM
			address &= 0x1fff;
			address |= 0x6000;
			boot_address_set(address);
/*			if((address & 0x8000) != 0){
				BUS_CONTROL_OUT = bit_get_negative(CPU_ROMCS);
			}*/
			BUS_CONTROL_OUT = BUS_CLOSE | (1 << CPU_PHI2);
		}
		direction_read();
		*data = DATABUS_IN;
		data += 1;
		BUS_CONTROL_OUT = BUS_CLOSE;
		address += 1;
		length--;
	}
	boot_address_set(ADDRESS_CLOSE);
}
