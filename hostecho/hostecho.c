#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <usb.h>
#include <kazzo_request.h>
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

static bool buf_load(uint8_t *buf, const char *file, int size)
{
	FILE *fp;

	fp = fopen(file, "rb");
	if(fp == NULL){
		return false;
	}
	fseek(fp, 0, SEEK_SET);
	fread(buf, sizeof(uint8_t), size, fp);
	fclose(fp);
	return true;
}

static void firmware_update(usb_dev_handle *handle, const char *file)
{
	uint8_t *firmware, *compare;
	const int firmsize = 0x1c00;
	assert(firmsize <= 0x2000);
	firmware = malloc(firmsize);
	memset(firmware, 0xff, firmsize);
	compare = malloc(firmsize);
	if(buf_load(firmware, file, firmsize) == false){
		puts("image open error!");
		goto end;
	}
	write_memory(handle, REQUEST_PPU_WRITE, 0, firmsize, firmware);
	read_memory(handle, REQUEST_PPU_READ, INDEX_IMPLIED, 0, firmsize, compare);
	if(memcmp(firmware, compare, firmsize) == 0){
		write_memory(handle, REQUEST_FIRMWARE_PROGRAM, 0, 1, firmware);
		//usb_close(handle);
	}else{
		puts("firmware transform error!");
	}
end:
	free(firmware);
	free(compare);
}

static void firmware_version(usb_dev_handle *handle)
{
	char version[VERSION_STRING_SIZE];
	read_memory(handle, REQUEST_FIRMWARE_VERSION, INDEX_IMPLIED, 0, VERSION_STRING_SIZE, (uint8_t *) version);
	puts(version);
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
		firmware_update(handle, v[2]);
		break;
	}
	usb_close(handle);
	return 0;
}
