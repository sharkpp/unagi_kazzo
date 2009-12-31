#ifndef _USBDEVICE_H_
#define _USBDEVICE_H_
usb_dev_handle *device_open(void);
void read_memory(usb_dev_handle *handle, const enum request r, enum index index, long address, long length, uint8_t *data);
void write_memory(usb_dev_handle *handle, enum request r, enum index index, long address, long length, const uint8_t *data);
#endif
