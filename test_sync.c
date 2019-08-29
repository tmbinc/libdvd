#include <stdio.h>
#include "libdvd.h"
#include <stdlib.h>

int main(void)
{
	struct sync_context s;
	sync_init(&s);
	int bits[] = {0,0,0,1,0,0,1,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1, 1,1,0,1,1,1,1,0,1,0,1,0,1,1,0,1,1,0,1,1,1,1,1,0,1,1,1,0,1,1,1,1};
	
	int i;
	for (i = 0; i < sizeof bits / sizeof *bits; ++i)
	{
		u16 data;
		int r = sync_put(&s, bits[i], &data);
		if (r)
			printf("%04x: %04x %d\n", i, data, r);
	}
	return 0;
}
