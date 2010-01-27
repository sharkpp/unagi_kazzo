#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "usbdrv.h"
#include "bus_access.h"
#include "disk_access.h"
#include "flashmemory.h"
#include "mcu_program.h"
#include "kazzo_request.h"

//---- global variable ----
#define REQUEST_NOP (0xee)
static struct write_command{
	enum request request;
	uint16_t address, length, offset;
}request_both_write, request_cpu_program, request_ppu_program;

//---- function start ----
/*static uint8_t cpu_buffer[FLASH_PACKET_SIZE];
static uint8_t ppu_buffer[FLASH_PACKET_SIZE];*/
uchar usbFunctionWrite(uchar *data, uchar len)
{
	static uint8_t cpu_buffer[FLASH_PACKET_SIZE];
	static uint8_t ppu_buffer[FLASH_PACKET_SIZE];
	const uint16_t length = (uint16_t) len;
	uchar i;
	//decode masked data
	for(i = 0; i < len; i++){
		data[i] ^= 0xa5;
	}
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
		uchar ret = request_both_write.offset == request_both_write.length;
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
		memcpy(cpu_buffer + request_cpu_program.offset, data, length);
		request_cpu_program.offset += length;
		uchar ret = request_cpu_program.offset == request_cpu_program.length;
		if(ret){
			if(request_cpu_program.request == REQUEST_FLASH_CONFIG_SET){
				flash_cpu_config(cpu_buffer, request_cpu_program.length);
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
		memcpy(ppu_buffer + request_ppu_program.offset, data, length);
		request_ppu_program.offset += length;
		uchar ret = request_ppu_program.offset == request_ppu_program.length;
		if(ret){
			if(request_ppu_program.request == REQUEST_FLASH_CONFIG_SET){
				flash_ppu_config(ppu_buffer, request_cpu_program.length);
			}else{
				flash_ppu_program(request_ppu_program.address, request_ppu_program.length, ppu_buffer);
			}
			request_ppu_program.request = REQUEST_NOP;
		}
		return ret;}
	default:
		break;
	}
	return 1;
}

//static uint8_t readbuffer[READ_PACKET_SIZE];
usbMsgLen_t usbFunctionSetup(uchar d[8])
{
	static uint8_t readbuffer[READ_PACKET_SIZE];
	static uint8_t status[2];
	usbRequest_t *rq = (void *)d;
	struct write_command *write_command;

	switch((enum request) rq->bRequest){
	case REQUEST_ECHO:
		readbuffer[0] = rq->wValue.bytes[0];
		readbuffer[1] = rq->wValue.bytes[1];
		readbuffer[2] = rq->wIndex.bytes[0];
		readbuffer[3] = rq->wIndex.bytes[1];
		usbMsgPtr = readbuffer;
		return 4;
	case REQUEST_PHI2_INIT:
		flash_both_idle();
		phi2_init();
		return 0;
	case REQUEST_CPU_READ:
		cpu_read(rq->wValue.word, rq->wLength.word, readbuffer);
		goto xxx_read;
	case REQUEST_CPU_READ_6502:
		cpu_read_6502(rq->wValue.word, rq->wLength.word, readbuffer);
		goto xxx_read;
	case REQUEST_PPU_READ:
		ppu_read(rq->wValue.word, rq->wLength.word, readbuffer);
		goto xxx_read;
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
/*	case REQUEST_FLASH_BUFFER_GET:
		if(rq->wIndex.word == INDEX_CPU){
			usbMsgPtr = cpu_buffer;
		}else{
			usbMsgPtr = ppu_buffer;
		}
		return FLASH_PACKET_SIZE;*/
	case REQUEST_DISK_STATUS_GET:
		//usbMsgPtr = status;
		return 0; //disk_status_get(status);
	case REQUEST_DISK_READ: 
		disk_init(DISK_READ);
		return 0;
	case REQUEST_DISK_WRITE:
		disk_init(DISK_WRITE);
		return 0;
	case REQUEST_FLASH_STATUS:
		usbMsgPtr = status;
		switch((enum index) rq->wIndex.word){
		case INDEX_CPU:
			status[0] = flash_cpu_status();
			return 1;
		case INDEX_PPU:
			status[0] = flash_ppu_status();
			return 1;
		default:
			status[0] = flash_cpu_status();
			status[1] = flash_ppu_status();
			return 2;
		}
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
	case REQUEST_FIRMWARE_VERSION:{
		__attribute__ ((section(".firmware.version")))
		static const /*PROGMEM*/ char date[VERSION_STRING_SIZE] = 
#if PCB_REVISION == 1
		"kazzo16"
#endif
#if PCB_REVISION == 2
		"kazzo^8"
#endif
		" 0.1.3 / " __DATE__;
		memcpy_P(readbuffer, date, rq->wLength.word);
		goto xxx_read;}
	case REQUEST_FIRMWARE_PROGRAM:{
		void (*t)(uint8_t *buf, uint16_t address, uint16_t length);
#if PCB_REVISION == 1
		static const char signature[] = {'k', 'a', 'z', 'z', 'o', '1', '6'};
#endif
#if PCB_REVISION == 2
		static const char signature[] = {'k', 'a', 'z', 'z', 'o', '^', '8'};
#endif
		const uint16_t address = rq->wValue.word;
		const uint16_t length = rq->wIndex.word;
		if(address >= 0x3800){
			return 0;
		}
		if(((address & 0x3f00) == 0) && (length < 0x1800)){
			return 0;
		}
		if(length + address > 0x3800){
			return 0;
		}
		cpu_read(0x3780 - 0x2000 + 0x6000, sizeof(signature), readbuffer);
		if(memcmp(readbuffer, signature, sizeof(signature)) == 0){
			usbDeviceDisconnect();
			memcpy_P(&t, &BOOTLOADER_ASSIGN.programmer, sizeof(BOOTLOADER_ASSIGN.programmer));
			(*t)(readbuffer, address, length);
		}
		}return 0;
	case REQUEST_FIRMWARE_DOWNLOAD:{
		const /*PROGMEM*/ uint8_t *firm = (const /*PROGMEM*/ uint8_t *) rq->wValue.word;
		memcpy_P(readbuffer, firm, rq->wLength.word);
		}
		goto xxx_read;
	xxx_read:
		usbMsgPtr = readbuffer;
		return rq->wLength.word;
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

	bus_init();
	usbInit();
	usbDeviceDisconnect();
	{
		uchar   i;
		i = 0;
		while(--i){
			wdt_reset();
			_delay_ms(1);
		}
	}
	usbDeviceConnect();
	wdt_enable(WDTO_500MS);
	sei();
	for(;;){
		wdt_reset();
		usbPoll();
		//disk_process();
		flash_process();
	}
	return 0;
}
