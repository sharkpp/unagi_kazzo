#include <stdint.h>
#include <util/delay.h>
#include "request.h"
#include "bus_access.h"

//---- global variable ----
struct flash_seqence{
	void (*const writer)(uint16_t address, uint16_t length, const uint8_t *data);
	void (*const reader)(uint16_t address, uint16_t length, uint8_t *data);
	enum compare_status (*const compare)(uint16_t address, uint16_t length, const uint8_t *data);
	enum{
		IDLE = 0, 
		ERASE, ERASE_WAIT,
		PROGRAM, TOGGLE_FIRST, TOGGLE_CHECK
	} status, request;
	uint16_t command_2aaa, command_5555;
	uint16_t address, length, program_unit;
	const uint8_t *data;
	uint8_t toggle;
};
static struct flash_seqence seqence_cpu = {
	.status = IDLE, .reader = cpu_read, .writer = cpu_write_flash,
	.compare = cpu_compare
};
static struct flash_seqence seqence_ppu = {
	.status = IDLE, .reader = ppu_read, .writer = ppu_write,
	.compare = ppu_compare
};

//---- task registration ----
uint8_t flash_cpu_status(void)
{
	return seqence_cpu.status;
}
uint8_t flash_ppu_status(void)
{
	return seqence_ppu.status;
}
static void config_set(uint16_t c2aaa, uint16_t c5555, uint16_t unit, struct flash_seqence *t)
{
	t->command_2aaa = c2aaa;
	t->command_5555 = c5555;
	t->program_unit = unit;
};
void flash_cpu_config(uint16_t c2aaa, uint16_t c5555, uint16_t unit)
{
	config_set(c2aaa, c5555, unit, &seqence_cpu);
}
void flash_ppu_config(uint16_t c2aaa, uint16_t c5555, uint16_t unit)
{
	config_set(c2aaa, c5555, unit, &seqence_ppu);
}

static void program_assign(uint16_t address, uint16_t length, const uint8_t *data, struct flash_seqence *t)
{
	t->address = address;
	t->length = length;
	t->data = data;
}
void flash_cpu_program(uint16_t address, uint16_t length, const uint8_t *data)
{
	seqence_cpu.status = PROGRAM;
	seqence_cpu.request = PROGRAM;
	program_assign(address, length, data, &seqence_cpu);
}
void flash_ppu_program(uint16_t address, uint16_t length, const uint8_t *data)
{
	seqence_ppu.status = PROGRAM;
	seqence_ppu.request = PROGRAM;
	program_assign(address, length, data, &seqence_ppu);
}
#define NULL (0)
void flash_cpu_erase(uint16_t address)
{
	seqence_cpu.status = ERASE;
	seqence_cpu.request = ERASE;
	//length に unit を渡して toggle check 後 IDLE になるようにする
	program_assign(address, seqence_cpu.program_unit, NULL, &seqence_cpu);
}
void flash_ppu_erase(uint16_t address)
{
	seqence_ppu.status = ERASE;
	seqence_ppu.request = ERASE;
	program_assign(address, seqence_ppu.program_unit, NULL, &seqence_ppu);
}

//---- command write ----
struct flash_command{
	enum {C2AAA, C5555, END} address;
	uint8_t data;
};
static void command_execute(const struct flash_command *c, const struct flash_seqence *const t)
{
	while(c->address != END){
		uint16_t addr = 0;
		switch(c->address){
		case C2AAA:
			addr = t->command_2aaa;
			break;
		case C5555:
			addr = t->command_5555;
			break;
		case END:
			return;
		}
		t->writer(addr, 1, &c->data);
		c++;
	}
}
static void program(struct flash_seqence *t)
{
	static const struct flash_command c[] = {
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0xa0}, {END, 0}
	};
	command_execute(c, t);
	t->writer(t->address, t->program_unit, t->data);
}

static void erase(struct flash_seqence *t)
{
	static const struct flash_command c[] = {
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0x80}, 
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0x10}, 
		{END, 0}
	};
	command_execute(c, t);
}

//---- status read ----
static void toggle_first(struct flash_seqence *t)
{
	t->reader(t->address, 1, &t->toggle);
	t->toggle &= 0x40;
}
static void toggle_check(struct flash_seqence *t)
{
	uint8_t d;
	t->reader(t->address, 1, &d);
	if(t->toggle == (d & 0x40)){
		if((t->program_unit != 1) && (t->request == PROGRAM)){ //page program device retry
			if(t->compare(t->address, t->program_unit, t->data) == NG){
				t->status = PROGRAM;
				return;
			}
		}
		t->address += t->program_unit;
		t->data += t->program_unit;
		t->length -= t->program_unit;
		if((t->length == 0) || (t->request == ERASE)){
			t->status = IDLE;
		}else{
			t->status = PROGRAM;
		}
	}
	t->toggle = d & 0x40;
	if(0 && (d & 0x20)){ //polling DQ5, AM29F040B only
		uint8_t d0, d1;
		t->reader(t->address, 1, &d0);
		t->reader(t->address, 1, &d1);
		if((d0 & 0x40) == (d1 & 0x40)){
			t->address += t->program_unit;
			t->data += t->program_unit;
			t->length -= t->program_unit;
			if((t->length == 0) || (t->request == ERASE)){
				t->status = IDLE;
			}else{
				t->status = PROGRAM;
			}
		}
	}
}

static void erase_wait(struct flash_seqence *t)
{
	uint8_t d;
	t->reader(t->address, 1, &d);
	if(d == 0xff){
		t->status = IDLE;
	}
}
//---- task execute ----
static void process(struct flash_seqence *s)
{
	switch(s->status){
	case IDLE:
		break;
	case ERASE:
		erase(s);
		s->status = ERASE_WAIT;
		break;
	case ERASE_WAIT:
		erase_wait(s);
		break;
	case PROGRAM:
		program(s);
		s->status = TOGGLE_FIRST;
		break;
	case TOGGLE_FIRST:
		toggle_first(s);
		s->status = TOGGLE_CHECK;
		break;
	case TOGGLE_CHECK:
		toggle_check(s); //status is updated by function
		break;
	}

}
void flash_process(void)
{
	//switchx2 is expantion, for CPU and PPU dual programming
	process(&seqence_cpu);
	process(&seqence_ppu);
}
