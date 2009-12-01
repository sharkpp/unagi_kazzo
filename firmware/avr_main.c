#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usbdrv.h"
#include "bus_access.h"
#include "disk_access.h"
#include "flashmemory.h"
#include "kazzo_request.h"

//---- global variable ----
static struct write_command{
	enum request request;
	uint16_t address, length, offset;
}write_command;

//---- function start ----
uchar usbFunctionWrite(uchar *data, uchar len)
{
	static uint8_t cpu_buffer[FLASH_PACKET_SIZE];
	static uint8_t ppu_buffer[FLASH_PACKET_SIZE];
	const uint16_t length = (uint16_t) len;
	
	switch(write_command.request){
	case REQUEST_CPU_WRITE_6502:
		cpu_write_6502(write_command.address + write_command.offset, length, data);
		break;
	case REQUEST_CPU_WRITE_FLASH:
		cpu_write_flash(write_command.address + write_command.offset, length, data);
		break;
	case REQUEST_PPU_WRITE:
		ppu_write(write_command.address + write_command.offset, length, data);
		break;
	case REQUEST_CPU_FLASH_PROGRAM:
	case REQUEST_PPU_FLASH_PROGRAM:{
		static uint8_t *w = cpu_buffer; //this is static pointer! be careful.
		if(write_command.offset == 0){
			if(write_command.request == REQUEST_CPU_FLASH_PROGRAM){
				w = cpu_buffer;
			}else{
				w = ppu_buffer;
			}
		}
		while(len != 0){
			*w = *data;
			w++;
			data++;
			write_command.offset += 1;
			len--;
		}
		if(write_command.length == write_command.offset){
			if(write_command.request == REQUEST_CPU_FLASH_PROGRAM){
				flash_cpu_program(write_command.address, write_command.length, cpu_buffer);
			}else{
				flash_ppu_program(write_command.address, write_command.length, ppu_buffer);
			}
		}
		}return write_command.length == write_command.offset;
	case REQUEST_CPU_FLASH_CONFIG_SET:
	case REQUEST_PPU_FLASH_CONFIG_SET:{
		static uint16_t c000x, c2aaa, c5555, unit;
		while(len != 0){
			switch(write_command.offset){
			case 0:
				c000x = *data;
				break;
			case 1:
				c000x |= *data;
				break;
			case 2:
				c2aaa = *data;
				break;
			case 3:
				c2aaa |= *data << 8;
				break;
			case 4:
				c5555 = *data;
				break;
			case 5:
				c5555 |= *data << 8;
				break;
			case 6:
				unit = *data;
				break;
			case 7:
				unit |= *data << 8;
				break;
			}
			data += 1;
			write_command.offset += 1;
			len--;
		}
		if(write_command.offset >= 8){
			if(write_command.request == REQUEST_CPU_FLASH_CONFIG_SET){
				flash_cpu_config(c000x, c2aaa, c5555, unit);
			}else{
				flash_ppu_config(c000x, c2aaa, c5555, unit);
			}
		}
		}return write_command.length == write_command.offset;
	default:
		return 1;
	}
	write_command.offset += length;
	return write_command.length == write_command.offset;
}

usbMsgLen_t usbFunctionSetup(uchar d[8])
{
	static uint8_t readbuffer[READ_PACKET_SIZE];
	static uint8_t status[2];
	usbRequest_t *rq = (void *)d;
	uint8_t *data = readbuffer;

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
	case REQUEST_CPU_FLASH_PROGRAM:
	case REQUEST_CPU_FLASH_CONFIG_SET:
	case REQUEST_PPU_FLASH_PROGRAM:
	case REQUEST_PPU_FLASH_CONFIG_SET:
		write_command.request = rq->bRequest;
		write_command.length = rq->wLength.word;
		write_command.address = rq->wValue.word;
		write_command.offset = 0;
		return USB_NO_MSG; //goto usbFunctionWrite
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
	case REQUEST_CPU_FLASH_STATUS:
		status[0] = flash_cpu_status();
		usbMsgPtr = status;
		return 1;
	case REQUEST_PPU_FLASH_STATUS:
		status[0] = flash_ppu_status();
		usbMsgPtr = status;
		return 1;
	case REQUEST_CPU_FLASH_DEVICE:
		flash_cpu_device_get(status);
		return 2;
	case REQUEST_PPU_FLASH_DEVICE:
		flash_ppu_device_get(status);
		return 2;
	case REQUEST_CPU_FLASH_ERASE:
		flash_cpu_erase(rq->wValue.word);
		return 0;
	case REQUEST_PPU_FLASH_ERASE:
		flash_ppu_erase(rq->wValue.word);
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
