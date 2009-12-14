#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "usbdrv.h"
#include "bus_access.h"
#include "disk_access.h"
#include "flashmemory.h"
#include "kazzo_request.h"

//---- global variable ----
#define REQUEST_NOP (0xee)
static struct write_command{
	enum request request;
	uint16_t address, length, offset;
}request_both_write, request_cpu_program, request_ppu_program;

//---- function start ----
static void flash_config_set(const uint8_t *t, void (*set)(uint16_t, uint16_t, uint16_t, uint16_t))
{
	uint16_t c000x, c2aaa, c5555, unit;
	c000x = t[0];
	c000x |= t[1] << 8;
	c2aaa = t[2];
	c2aaa |= t[3] << 8;
	c5555 = t[4];
	c5555 |= t[5] << 8;
	unit = t[6];
	unit |= t[7] << 8;
	(*set)(c000x, c2aaa, c5555, unit);
}
static uint8_t cpu_buffer[FLASH_PACKET_SIZE];
static uint8_t ppu_buffer[FLASH_PACKET_SIZE];
uchar usbFunctionWrite(uchar *data, uchar len)
{
//	static uint8_t cpu_buffer[FLASH_PACKET_SIZE];
//	static uint8_t ppu_buffer[FLASH_PACKET_SIZE];
	const uint16_t length = (uint16_t) len;
	
	switch(request_both_write.request){
	case REQUEST_CPU_WRITE_6502:
		cpu_write_6502(request_both_write.address + request_both_write.offset, length, data);
		goto BOTH_NEXT;
	case REQUEST_CPU_WRITE_FLASH:
		cpu_write_flash(request_both_write.address + request_both_write.offset, length, data);
		goto BOTH_NEXT;
	case REQUEST_PPU_WRITE:
		ppu_write(request_both_write.address + request_both_write.offset, length, data);
		goto BOTH_NEXT;
	BOTH_NEXT:{
		request_both_write.offset += length;
		int ret = request_both_write.offset == request_both_write.length;
		if(ret){
			request_both_write.request = REQUEST_NOP;
		}
		return ret;
		}
	default:
		break;
	}
	switch(request_cpu_program.request){
	case REQUEST_FLASH_PROGRAM:
	case REQUEST_FLASH_CONFIG_SET:{
		static uint8_t *w = cpu_buffer; //this is static pointer! be careful.
		if(request_cpu_program.offset == 0){
			w = cpu_buffer;
		}
		memcpy(w, data, length);
		w += length;
		request_cpu_program.offset += length;
		int ret = request_cpu_program.offset >= request_cpu_program.length;
		if(ret){
			if(request_cpu_program.request == REQUEST_FLASH_CONFIG_SET){
				flash_config_set(cpu_buffer, flash_cpu_config);
			}else{
				flash_cpu_program(request_cpu_program.address, request_cpu_program.length, cpu_buffer);
			}
			request_cpu_program.request = REQUEST_NOP;
		}
		return ret;}
	default:
		break;
	}
	switch(request_ppu_program.request){
	case REQUEST_FLASH_PROGRAM:
	case REQUEST_FLASH_CONFIG_SET:{
		static uint8_t *w = ppu_buffer; //static pointer
		if(request_ppu_program.offset == 0){
			w = ppu_buffer;
		}
		memcpy(w, data, length);
		w += length;
		request_ppu_program.offset += length;
		int ret = request_ppu_program.offset >= request_ppu_program.length;
		if(ret){
			if(request_ppu_program.request == REQUEST_FLASH_CONFIG_SET){
				flash_config_set(ppu_buffer, flash_ppu_config);
			}else{
				flash_ppu_program(request_ppu_program.address, request_ppu_program.length, ppu_buffer);
			}
			request_ppu_program.request = REQUEST_NOP;
		}
		return ret;}
	default:
		break;
	}
	return 1; //when returns 0, sometime occours USB commnunication Error
}

usbMsgLen_t usbFunctionSetup(uchar d[8])
{
	static uint8_t readbuffer[READ_PACKET_SIZE];
	static uint8_t status[2];
	usbRequest_t *rq = (void *)d;
	uint8_t *data = readbuffer;
	struct write_command *write_command;

	switch((enum request) rq->bRequest){
	case REQUEST_ECHO:
		data[0] = rq->wValue.bytes[0];
		data[1] = rq->wValue.bytes[1];
		data[2] = rq->wIndex.bytes[0];
		data[3] = rq->wIndex.bytes[1];
		usbMsgPtr = data;
		return 4;
	case REQUEST_PHI2_INIT:
		flash_both_idle();
		phi2_init();
		return 0;
	case REQUEST_CPU_READ:
		cpu_read(rq->wValue.word, rq->wLength.word, data);
		goto xxx_read;
	case REQUEST_CPU_READ_6502:
		cpu_read_6502(rq->wValue.word, rq->wLength.word, data);
		goto xxx_read;
	case REQUEST_PPU_READ:
		ppu_read(rq->wValue.word, rq->wLength.word, data);
		goto xxx_read;
	xxx_read:
		usbMsgPtr = data;
		return rq->wLength.word;
	case REQUEST_CPU_WRITE_6502: case REQUEST_CPU_WRITE_FLASH:
	case REQUEST_PPU_WRITE:
		write_command = &request_both_write;
		goto xxx_write;
	case REQUEST_FLASH_PROGRAM:
	case REQUEST_FLASH_CONFIG_SET:
		if(rq->wIndex.word == INDEX_CPU){
			write_command = &request_cpu_program;
		}else{
			write_command = &request_ppu_program;
		}
		goto xxx_write;
	xxx_write:
		write_command->request = rq->bRequest;
		write_command->length = rq->wLength.word;
		write_command->address = rq->wValue.word;
		write_command->offset = 0;
		return USB_NO_MSG; //goto usbFunctionWrite
	case REQUEST_FLASH_BUFFER_GET:
		if(rq->wIndex.word == INDEX_CPU){
			usbMsgPtr = cpu_buffer;
		}else{
			usbMsgPtr = ppu_buffer;
		}
		return FLASH_PACKET_SIZE;
	case REQUEST_DISK_STATUS_GET:
		usbMsgPtr = status;
		return disk_status_get(status);
	case REQUEST_DISK_READ: 
		disk_init(DISK_READ);
		return 0;
	case REQUEST_DISK_WRITE:
		disk_init(DISK_WRITE);
		return 0;
	case REQUEST_BOTH_FLASH_STATUS:
		status[0] = flash_cpu_status();
		status[1] = flash_ppu_status();
		usbMsgPtr = status;
		return 2;
	case REQUEST_FLASH_STATUS:
		if(rq->wIndex.word == INDEX_CPU){
			status[0] = flash_cpu_status();
		}else{
			status[0] = flash_ppu_status();
		}
		usbMsgPtr = status;
		return 1;
	case REQUEST_FLASH_DEVICE:
		if(rq->wIndex.word == INDEX_CPU){
			flash_cpu_device_get(status);
		}else{
			flash_ppu_device_get(status);
		}
		usbMsgPtr = status;
		return 2;
	case REQUEST_FLASH_ERASE:
		if(rq->wIndex.word == INDEX_CPU){
			flash_cpu_erase(rq->wValue.word);
		}else{
			flash_ppu_erase(rq->wValue.word);
		}
		return 0;
	case REQUEST_VRAM_CONNECTION:
		status[0] = vram_connection_get();
		usbMsgPtr = status;
		return 1;
	}
	return 0;
}

int main(void)
{
	static const struct write_command wc_init = {
		.request = REQUEST_NOP, .length = 0, .offset = 0
	};
	request_both_write = wc_init;
	request_cpu_program = wc_init;
	request_ppu_program = wc_init;
	uchar   i;

	bus_init();
	usbInit();
	usbDeviceDisconnect();
	i = 0;
	while(--i){
		wdt_reset();
		_delay_ms(1);
	}
	usbDeviceConnect();
	sei();
	for(;;){
		wdt_reset();
		usbPoll();
		//disk_process();
		flash_process();
	}
	return 0;
}
