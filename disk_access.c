#include <avr/interrupt.h>
#include <util/delay.h>
#include "bus_access.h"
#include "disk_access.h"

uint16_t disk_status_get(uint8_t *data)
{
	const uint16_t length = 2;
	cpu_read(0x4030, length, data);
	return length;
}
static enum sequence{
	IDLE = 0, 
	INIT_MOTOR_STOP, INIT_BATTERY, INIT_MOTOR_START,
	WAIT_READY,
	READ_START, READING, READ_END
} sequence = IDLE;

void disk_init(enum DISK_REQUEST r)
{
	sequence = INIT_MOTOR_STOP;
}

enum disk_control{ //$4025 control bitfield assignment
	MOTOR = 0, //0:stop, 1:work
	TRANSFER_RESET, //1:do reset
	DIRECTION, //0:write, 1:read
	VRAM_MIRRORING,
	BLOCK_END_MARK,
	UNKOWN, //always 1
	ACCESS_START,
	INTERRUPT //0:off, 1:on
};
static inline uint8_t bit_set(enum disk_control bit)
{
	return 1 << bit;
};
enum{
	BUFFER_ADDRESS = 0x0000,
	DRQ_COUNT = 0x0400,
	DISK_CONTROL = 0x4025
};
static void data_buffer_init(void)
{
	const uint8_t filldata[] = {
		0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 
		0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55
	};
	uint16_t address = BUFFER_ADDRESS;
	while(address < BUFFER_ADDRESS + DRQ_COUNT){
		ppu_write(address, sizeof(filldata), filldata);
		address += sizeof(filldata);
	}
}

static inline void motor_stop(void)
{
	uint8_t w;
	w = bit_set(UNKOWN) | bit_set(DIRECTION) | bit_set(TRANSFER_RESET);
	cpu_write_6502(DISK_CONTROL, 1, &w);
}

static inline void motor_start(void)
{
	uint8_t w;
	w = bit_set(UNKOWN) | bit_set(DIRECTION) | bit_set(TRANSFER_RESET)| bit_set(MOTOR);
	cpu_write_6502(DISK_CONTROL, 1, &w);
	clock_wait(9);
	w = bit_set(UNKOWN) | bit_set(DIRECTION) | bit_set(MOTOR);
	cpu_write_6502(DISK_CONTROL, 1, &w);
}

volatile static uint16_t drq_count;
void disk_process(void)
{
	const uint16_t status = 0x4032;
	switch(sequence){
	default:
	case IDLE:
		break;
	case INIT_MOTOR_STOP:{
		const uint8_t writebuf[] = {0, 0, 0, 0x83};
		//GICR &= ~bit_set(INT1);
		data_buffer_init();
		//timer interrupt off, disk IO on
		cpu_write_6502(0x4020, sizeof(writebuf), writebuf);
		motor_stop();
		_delay_ms(0x200);
		sequence = INIT_BATTERY;
		}break;
	case INIT_BATTERY:{
		motor_start();
		_delay_ms(150);
		uint8_t w = 0xff;
		cpu_write_6502(0x4026, 1, &w);
		sequence = INIT_MOTOR_START;
		}
		break;
	case INIT_MOTOR_START:{
		motor_stop();
		motor_start();
		sequence = WAIT_READY;
		}break;
	case WAIT_READY:{
		uint8_t s;
		cpu_read_6502(status, 1, &s);
		if((s & 0b10) == 0b00){
			sequence = READ_START;
		}
		}break;
	case READ_START:{
		uint8_t w;
		_delay_ms(267 + 5);
		
		w = bit_set(ACCESS_START) | bit_set(UNKOWN) | bit_set(VRAM_MIRRORING) | bit_set(DIRECTION) | bit_set(MOTOR);
		cpu_write_6502(DISK_CONTROL, 1, &w);
		clock_wait(21 - 1);
		//DRQ enable
		//MCU INT1 init
		GICR |= bit_set(INT1);
		drq_count = DRQ_COUNT;
		//RP2C33 IRQ enable
		w |= bit_set(INTERRUPT);
		cpu_write_6502(DISK_CONTROL, 1, &w);
		sequence = READING;
		}break;
	case READING:
		if(drq_count == 0){
			sequence = READ_END;
		}
		break;
	case READ_END:{
/*		uint8_t w;
		w = bit_set(ACCESS_START) | bit_set(UNKOWN) | bit_set(DIRECTION) | bit_set(MOTOR);
		cpu_write_6502(DISK_CONTROL, 1, &w);*/
		GICR &= ~bit_set(INT1);
		
		sequence = IDLE;
		}break;
	}
	uint8_t w[2];
	w[0] = sequence;
	w[1] = 0x55;
	ppu_write(0x0aaa, 2, w);
	phi2_update();
}

ISR(INT1_vect)
{
	static uint16_t buffer_pointer = BUFFER_ADDRESS;
	uint8_t d;
	cpu_read_6502(0x4031, 1, &d);
	ppu_write(buffer_pointer++, 1, &d);
	//cpu_write_6502(buffer_pointer++, 1, &d);
	if(buffer_pointer >= BUFFER_ADDRESS + DRQ_COUNT){
		buffer_pointer = 0;
	}
	drq_count -= 1;
}
