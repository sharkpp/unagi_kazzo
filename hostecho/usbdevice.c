#include <stdio.h>
#include <stdint.h>
#include <usb.h>
#include <kazzo_request.h>
#include "opendevice.h"
#include "usbconfig.h"

enum{
	TIMEOUT = 4000
};
usb_dev_handle *device_open(void)
{
	usb_dev_handle *handle = NULL;
	const unsigned char rawVid[2] = {USB_CFG_VENDOR_ID};
	const unsigned char rawPid[2] = {USB_CFG_DEVICE_ID};
	char vendor[] = {USB_CFG_VENDOR_NAME, 0};
	char product[] = {USB_CFG_DEVICE_NAME, 0};
	int vid, pid;

	/* compute VID/PID from usbconfig.h so that there is a central source of information */
	vid = (rawVid[1] << 8) | rawVid[0];
	pid = (rawPid[1] << 8) | rawPid[0];

	if(usbOpenDevice(&handle, vid, vendor, pid, product, NULL, NULL, NULL) == 0){
		return handle;
	}
	fprintf(stderr, "Could not find USB device \"%s\" with vid=0x%x pid=0x%x\n", product, vid, pid);
	return NULL;
}

//-------- read sequence --------
static void device_read(usb_dev_handle *handle, enum request r, enum index index, long address, long length, uint8_t *data)
{
	int cnt = usb_control_msg(
		handle, 
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
		r, address, 
		index, data, length, TIMEOUT
	);
	if(cnt != length){
		puts(__FUNCTION__);
		puts(usb_strerror());
		exit(1);
	}
}
void read_memory(usb_dev_handle *handle, const enum request r, enum index index, long address, long length, uint8_t *data)
{
	const int packet = READ_PACKET_SIZE;
	while(length >= packet){
		device_read(handle, r, index, address, packet, data);
		data += packet;
		address += packet;
		length -= packet;
	}
	if(length != 0){
		device_read(handle, r, index, address, length, data);
	}
}
//-------- write sequence --------
/*
When host send data that contains 0xff many times, v-usb losts some 
bits. To prevent losting bits, mask data xor 0xa5;
*/
static void device_write(usb_dev_handle *handle, enum request w, enum index index, long address, long length, const uint8_t *data)
{
	uint8_t *d = malloc(length);
	int i;
	memcpy(d, data, length);
	for(i = 0; i < length; i++){
		d[i] ^= 0xa5;
	}
	int cnt = usb_control_msg(
		handle, 
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
		w, address, 
		index, d, length, TIMEOUT
	);
	if(cnt != length){
		puts(__FUNCTION__);
		puts(usb_strerror());
		exit(1);
	}
	free(d);
}

void write_memory(usb_dev_handle *handle, enum request r, long address, long length, const uint8_t *data)
{
	while(length != 0){
		long l = length >= FLASH_PACKET_SIZE ? FLASH_PACKET_SIZE : length;
		device_write(handle, r, INDEX_IMPLIED, address, l, data);
		address += l;
		data += l;
		length -= l;
	}
}
