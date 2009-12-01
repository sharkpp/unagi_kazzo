#ifndef _DISK_ACCESS_H_
#define _DISK_ACCESS_H_
enum DISK_REQUEST{
	DISK_READ, DISK_WRITE
};
uint16_t disk_status_get(uint8_t *data);
void disk_init(enum DISK_REQUEST r);
void disk_process(void);
#endif
