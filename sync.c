#include "libdvd.h"
#include <stdio.h>

void sync_init(struct sync_context *c)
{
	c->data = 0;
	c->last = 0;
	c->b = -1;
}

		/* sync codes for acquiring bit-sync */
const u32 syncword[2*8*2] =
{
	0x12440011, 0x12040011, // SY0
  0x04040011, 0x04440011, // SY1
	0x10040011, 0x10440011, // SY2
	0x08040011, 0x08440011, // SY3
	0x20040011, 0x20440011, // SY4 
	0x22440011, 0x22040011, // SY5
	0x24840011, 0x20840011, // SY6
	0x24440011, 0x24040011, // SY7
	0x92040011, 0x92440011, // SY0
	0x84440011, 0x84040011, // SY1
	0x90440011, 0x90040011, // SY2
	0x82440011, 0x82040011, // SY3
	0x88440011, 0x88040011, // SY4
	0x89040011, 0x81040011, // SY5
	0x90840011, 0x80440011, // SY6
	0x88840011, 0x80840011, // SY7
};


int sync_identify(u32 code)
{
	int i;
	for (i = 0; i < 2 * 8 * 2; ++i)
		if (syncword[i] == code)
			return i;
	return -1;
}

int sync_put(struct sync_context *c, int bit, u16 *data)
{
  bit = !!bit;
	c->data <<= 1;

	//
  // NRZI
  //
	c->data |= bit != c->last;
	c->last = bit;

	int sy = -1;
	
	if ((c->data & 0xFFFF) == 0x0011)
	  sy = sync_identify(c->data);

	if (sy >= 0)
	{
    c->b = 0;
	}

	if (c->b < 0)
	  return 0; // not yet synced

	if ((c->b & 15) == 0)
	{
	  *data = c->data >> 16;
  } else
  {
    c->b++;
    return 0; // waiting for more bits
  }
  
  switch (c->b++ / 16)
  {
  case 0:
    return SYNC_VALID | SYNC_RESYNC;
  case 1:
    return SYNC_VALID;
  case 2:
    return SYNC_VALID | SYNC_DATA | SYNC_DATA_FIRST;
  default:
    return SYNC_VALID | SYNC_DATA;
  }
}

u32 sync_get(int sy, int state, int alt)
{
  return syncword[(sy + state * 8) * 2 + alt];
}
