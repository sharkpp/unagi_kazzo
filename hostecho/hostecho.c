#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <usb.h>
#include <kazzo_request.h>
#include "ihex.h"
#include "file.h"
#include "usbdevice.h"

static void echo(usb_dev_handle *handle)
{
	char buffer[4];
	int i, cnt;
	srand(4649);
	for(i = 0; i < 5000; i++){
		int value = rand() & 0xffff, index = rand() & 0xffff;
		int rxValue, rxIndex;
		if((i+1) % 100 == 0){
			fprintf(stderr, "\r%05d", i+1);
			fflush(stderr);
		}
		cnt = usb_control_msg(
			handle, 
			USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
			REQUEST_ECHO, value, 
			index, buffer, sizeof(buffer), 4000
		);
		if(cnt < 0){
			fprintf(stderr, "\nUSB error in iteration %d: %s\n", i, usb_strerror());
			break;
		}else if(cnt != 4){
			fprintf(stderr, "\nerror in iteration %d: %d bytes received instead of 4\n", i, cnt);
			break;
		}
		rxValue = ((int)buffer[0] & 0xff) | (((int)buffer[1] & 0xff) << 8);
		rxIndex = ((int)buffer[2] & 0xff) | (((int)buffer[3] & 0xff) << 8);
		if(rxValue != value || rxIndex != index){
			fprintf(stderr, "\ndata error in iteration %d:\n", i);
			fprintf(stderr, "rxValue = 0x%04x value = 0x%04x\n", rxValue, value);
			fprintf(stderr, "rxIndex = 0x%04x index = 0x%04x\n", rxIndex, index);
		}
	}
	printf("\nTest completed.\n");
}

static bool hex_load(const char *file, long firmsize, uint8_t *image)
{
	FILE *f;
	char s[0x80];
	f = fopen(file, "r");
	if(f == NULL){
		return false;
	}
	fseek(f, 0, SEEK_SET);
	
	struct record *t = ihex_new();
	bool ret = true;
	while(fgets(s, 0x80, f) != NULL){
		if(ihex_load(s, t) == false){
			ret = false;
			break;
		}
		ihex_write(t, firmsize, image);
	}
	ihex_destory(t);
	return ret;
}
static void snrom_wram_open(usb_dev_handle *handle)
{
	uint8_t t[5];
	t[0] = 0x80;
	write_memory(handle, REQUEST_CPU_WRITE_6502, INDEX_IMPLIED, 0x8000, 1, t);
	t[0] = 0, t[1] = 0, t[2] = 0, t[3] = 1, t[4] = 0;
	write_memory(handle, REQUEST_CPU_WRITE_6502, INDEX_IMPLIED, 0x8000, sizeof(t), t);
	t[0] = 0, t[1] = 0, t[2] = 0, t[3] = 0, t[4] = 0;
	write_memory(handle, REQUEST_CPU_WRITE_6502, INDEX_IMPLIED, 0xe000, sizeof(t), t);
	write_memory(handle, REQUEST_CPU_WRITE_6502, INDEX_IMPLIED, 0xa000, sizeof(t), t);
}

static void snrom_wram_close(usb_dev_handle *handle)
{
	const uint8_t t[5] = {0, 0, 0, 0, 1}; //lsb -> msb
	write_memory(handle, REQUEST_CPU_WRITE_6502, INDEX_IMPLIED, 0xe000, sizeof(t), t);
	write_memory(handle, REQUEST_CPU_WRITE_6502, INDEX_IMPLIED, 0xa000, sizeof(t), t);
}

static int cartridge_ram_transform(usb_dev_handle *handle, const uint8_t *firmware, enum request w, enum request r, long address, long length, bool dump)
{
	uint8_t *compare;
	compare = malloc(length);
	write_memory(handle, w, INDEX_IMPLIED, address, length, firmware);
	read_memory(handle, r, INDEX_IMPLIED, address, length, compare);
	int ret = memcmp(firmware, compare, length);
	if(dump == true){
		int i;
		uint8_t *t = compare;
		for(i = 0; i < length; i += 0x10){
			int j;
			printf("%06x:", i);
			for(j = 0; j < 0x10; j++){
				const char *safix;
				switch(j){
				case 7:
					safix = "-";
					break;
				case 0x0f:
					safix = "\n";
					break;
				default:
					safix = " ";
					break;
				}
				printf("%02x%s", *t, safix);
				t++;
			}
		}
	}
	free(compare);
	return ret;
}
static void firmware_update(usb_dev_handle *handle, const char *file)
{
	uint8_t *firmware;
	const int firmsize = 0x3800;
	assert(firmsize <= 0x3800);
	firmware = malloc(firmsize);
	memset(firmware, 0xff, firmsize);

	if(hex_load(file, firmsize, firmware) == false){
		puts("image open error!");
		goto end;
	}
	snrom_wram_open(handle);

	int ppu, cpu = 0;
	ppu = cartridge_ram_transform(handle, firmware, REQUEST_PPU_WRITE, REQUEST_PPU_READ, 0x0000, 0x2000, false);
	if(firmsize >= 0x2000){
		cpu = cartridge_ram_transform(handle, firmware + 0x2000, REQUEST_CPU_WRITE_6502, REQUEST_CPU_READ, 0x6000, firmsize - 0x2000, false);
	}
	if((ppu == 0) && (cpu == 0)){
//		write_memory(handle, REQUEST_FIRMWARE_PROGRAM, firmsize, 0x2000, 0, firmware);
		puts("USB connection will be disconnteced. This is normally.");
		puts("Re-turn on kazzo's power.");
		write_memory(handle, REQUEST_FIRMWARE_PROGRAM, firmsize, 0x0000, 0, firmware);
	}else{
		puts("firmware transform error!");
		snrom_wram_close(handle);
	}
end:
	free(firmware);
}
enum{
	FIRM_VERSION_OFFSET = 0x3780,
	BOOTLOADER_VERSION_OFFSET = 0x3d00
};
static void firmware_verify(usb_dev_handle *handle, const char *file)
{
	uint8_t *firmware, *compare;
	const int firmsize = 0x3800;
	assert(firmsize <= 0x3800);
	firmware = malloc(firmsize);
	compare = malloc(firmsize);
	memset(compare, 0xff, firmsize);
//	if(buf_load(compare, file, firmsize) == false){
	if(hex_load(file, firmsize, compare) == false){
		puts("image open error!");
		goto end;
	}
	read_memory(handle, REQUEST_FIRMWARE_DOWNLOAD, INDEX_IMPLIED, 0, firmsize, firmware);
	if(memcmp(firmware, compare, firmsize) == 0){
		puts("firmware compare ok!");
	}else{
		puts("firmware compare ng!");
		printf("hex: %s\n", compare + FIRM_VERSION_OFFSET);
		printf("avr: %s\n", firmware + FIRM_VERSION_OFFSET);
	}
end:
	free(firmware);
	free(compare);
}
static void firmware_download(usb_dev_handle *handle, const char *file)
{
	const int firmsize = 0x4000;
	uint8_t *firmware = malloc(firmsize);
	read_memory(handle, REQUEST_FIRMWARE_DOWNLOAD, INDEX_IMPLIED, 0, firmsize, firmware);
	buf_save(firmware, file, firmsize);
	puts(firmware + FIRM_VERSION_OFFSET);
	free(firmware);
}

static void firmware_version(usb_dev_handle *handle)
{
	char version[VERSION_STRING_SIZE];
	read_memory(handle, REQUEST_FIRMWARE_VERSION, INDEX_IMPLIED, 0, VERSION_STRING_SIZE, (uint8_t *) version);
	puts(version);
}

static bool ppu_read(usb_dev_handle *handle, int seed)
{
	int i;
	int ret = 0;
	const int bufsize = 0x80;
	uint8_t *t, *buf = malloc(bufsize);
/*	t = buf;
	for(i = 0; i < bufsize; i++){
		*t = i;
		t++;
	}
	ret |= cartridge_ram_transform(handle, buf, REQUEST_PPU_WRITE, REQUEST_PPU_READ, 0x0000, bufsize, false);*/
/*	if(ret == 0){
		puts("increment ok");
	}else{
		puts("increment ng");
	}*/

	t = buf;
	srand(5555 * seed);
	for(i = 0; i < bufsize; i++){
		*t = rand() & 0xff;
		t++;
	}
	ret |= cartridge_ram_transform(handle, buf, REQUEST_PPU_WRITE, REQUEST_PPU_READ, 0x0000, bufsize, false);
/*	if(ret == 0){
		puts("ramdom ok");
	}else{
		puts("ramdom ng");
	}*/
	t = buf;
	free(buf);
	return ret == 0;
}
int main(int c, char **v)
{
	usb_init();
	usb_dev_handle *handle = device_open();
	
	if(handle == NULL){
		return 0;
	}
	switch(c){
	case 1:
		echo(handle);
		printf("work time %3.1f second\n", ((double) clock() / (double) CLOCKS_PER_SEC));
		fflush(stdout);
		system("pause");
		break;
	case 2:
		firmware_version(handle);
		break;
	case 3:
		switch(v[1][0]){
		case 'w':
			firmware_update(handle, v[2]);
			break;
		case 'r':
			firmware_download(handle, v[2]);
			break;
		case 'v':
			firmware_verify(handle, v[2]);
			break;
		case 'p':{
			if(DEBUG == 0){
				break;
			}
			int i = 0x1000;
			const char *ret = "test ok";
			while(i != 0){
				if(ppu_read(handle, i) == false){
					ret = "test ng";
					break;
				}
				if((i+1) % 100 == 0){
					fprintf(stderr, "\r%05d", i+1);
					fflush(stderr);
				}
				i--;
			}
			puts(ret);
			}break;
		case 'd':
			if(DEBUG == 1){
				write_memory(handle, REQUEST_FIRMWARE_PROGRAM, 0x200, 0x0000, 0, NULL);
			}
			break;
		}
		break;
	}
	usb_close(handle);
	return 0;
}
