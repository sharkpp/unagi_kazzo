#ifndef _TYPE_H_
#define _TYPE_H_
static inline uint8_t bit_to_data(uint8_t data, int bit)
{
	data &= 1;
	return data << bit;
}
static inline uint8_t bit_get(uint8_t data, int bit)
{
	data >>= bit;
	return data & 1;
}
#endif
