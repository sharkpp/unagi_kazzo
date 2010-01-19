#include <stdint.h>
//#include <avr/interrupt.h>
#include "kazzo_task.h"
#include "bus_access.h"

//---- global variable ----
struct flash_seqence{
	void (*const writer)(uint16_t address, uint16_t length, const uint8_t *data);
	void (*const programmer)(const struct flash_order *t);
	void (*const reader)(uint16_t address, uint16_t length, uint8_t *data);
	enum compare_status (*const compare)(uint16_t address, uint16_t length, const uint8_t *data);
	enum status{
		IDLE = KAZZO_TASK_FLASH_IDLE, 
		ERASE, ERASE_WAIT,
		PROGRAM, TOGGLE_FIRST, TOGGLE_CHECK
	} status, request;
	uint16_t command_000x, command_2aaa, command_5555;
	uint8_t retry_enable;
	uint16_t address, length, program_unit;
	const uint8_t *data;
	uint8_t toggle, retry_count;
	struct flash_order program_command[FLASH_PROGRAM_ORDER];
};
static struct flash_seqence seqence_cpu = {
	.status = IDLE, .reader = cpu_read, 
	.writer = cpu_write_flash, .programmer = cpu_write_flash_order,
	.compare = cpu_compare
};
static struct flash_seqence seqence_ppu = {
	.status = IDLE, .reader = ppu_read, 
	.writer = ppu_write, .programmer = ppu_write_order,
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
void flash_both_idle(void)
{
	seqence_cpu.status = IDLE;
	seqence_ppu.status = IDLE;
}
static inline uint16_t unpack_short_le(const uint8_t *t)
{
	uint16_t r = t[0];
	r |= t[1] << 8;
	return r;
}
static void config_set(const uint8_t *data, struct flash_seqence *t)
{
	t->command_000x = unpack_short_le(data);
	data += 2;
	t->command_2aaa = unpack_short_le(data);
	data += 2;
	t->command_5555 = unpack_short_le(data);
	data += 2;
	t->program_unit = unpack_short_le(data);
	data += 2;
	t->retry_enable = *data;

	t->program_command[0].address = t->command_5555;
	t->program_command[0].data = 0xaa;
	t->program_command[1].address = t->command_2aaa;
	t->program_command[1].data = 0x55;
	t->program_command[2].address = t->command_5555;
	t->program_command[2].data = 0xa0;
};
void flash_cpu_config(const uint8_t *data)
{
	config_set(data, &seqence_cpu);
}
void flash_ppu_config(const uint8_t *data)
{
	config_set(data, &seqence_ppu);
}

static void program_assign(enum status status, uint16_t address, uint16_t length, const uint8_t *data, struct flash_seqence *t)
{
	if(0 && (t->program_unit != 1) && (t->status == PROGRAM)){ //W29C040 の再書き込み回数を減らしてみる
		t->status = TOGGLE_FIRST;
	}else{
		t->status = status;
	}
	t->request = status;
	t->address = address;
	t->length = length;
	t->data = data;
	t->retry_count = 0;
}
void flash_cpu_program(uint16_t address, uint16_t length, const uint8_t *data)
{
	program_assign(PROGRAM, address, length, data, &seqence_cpu);
}
void flash_ppu_program(uint16_t address, uint16_t length, const uint8_t *data)
{
	program_assign(PROGRAM, address, length, data, &seqence_ppu);
}
#define NULL (0)
void flash_cpu_erase(uint16_t address)
{
	//length に unit を渡して toggle check 後 IDLE になるようにする
	program_assign(ERASE, address, seqence_cpu.program_unit, NULL, &seqence_cpu);
}
void flash_ppu_erase(uint16_t address)
{
	program_assign(ERASE, address, seqence_ppu.program_unit, NULL, &seqence_ppu);
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
static void program(const struct flash_seqence *t)
{
/*	static const struct flash_command c[] = {
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0xa0}, {END, 0}
	};
	command_execute(c, t);*/
	t->programmer(t->program_command);
	t->writer(t->address, t->program_unit, t->data);
}

static void erase(const struct flash_seqence *t)
{
	static const struct flash_command c[] = {
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0x80}, 
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0x10}, 
		{END, 0}
	};
	command_execute(c, t);
}

static void device_get(const struct flash_seqence *t, uint8_t d[2])
{
	static const struct flash_command entry[] = {
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0x90}, {END, 0}
	};
	static const struct flash_command exit[] = {
		{C5555, 0xaa}, {C2AAA, 0x55}, {C5555, 0xf0}, {END, 0}
	};
	command_execute(entry, t);
	t->reader(t->command_000x, 1, d);
	command_execute(entry, t);
	t->reader(t->command_000x + 1, 1, d + 1);
	command_execute(exit, t);
}
void flash_cpu_device_get(uint8_t d[2])
{
	device_get(&seqence_cpu, d);
}
void flash_ppu_device_get(uint8_t d[2])
{
	device_get(&seqence_ppu, d);
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
		if((t->retry_enable != 0) && (t->request == PROGRAM) && (t->retry_count < 20)){
			if(t->compare(t->address, t->program_unit, t->data) == NG){
				t->retry_count += 1;
				t->status = PROGRAM;
				return;
			}
		}
		t->retry_count = 0;
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
		if((s->program_unit != 1) || (*(s->data) != 0xff)){
			program(s);
		}
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
	//for CPU and PPU dual programming
	process(&seqence_cpu);
	process(&seqence_ppu);
}
