#include "libdvd.h"
#include <stdio.h>

void nrzi_init(struct nrzi_context *nrzi)
{
	nrzi->nrzi_state = 0;
	nrzi->dist = 9;
}

u16 nrzi_encode(struct nrzi_context *nrzi, u16 nrz)
{
	u16 res = 0;
	int k;
	
	// first bit is k=15, so MSB goes first
	
	int nrzi_state = nrzi->nrzi_state;
	
#ifdef LIBDVD_LSB_FIRST
	for (k = 15; k >= 0; --k)
	{
		int m = 1<<k;
		int b = nrz & m;
		nrzi->dist++;
		if (b)
		{
			if (nrzi->dist < 3)
			{
				printf("INVALID DIST, last encode=%04x\n", nrz);
				*(int*)0=0;
			}
			nrzi_state = !nrzi_state;
			nrzi->dist = 0;
		}
		res >>= 1;
		res |= nrzi_state ? 0x8000 : 0;
	}
#else
	for (k = 15; k >= 0; --k)
	{
		int m = 1<<k;
		int b = nrz & m;
		dist++;
		if (b)
		{
			if (dist < 3)
			{
				printf("INVALID DIST\n");
				exit(1);
			}
			nrzi_state = !nrzi_state;
			dist = 0;
		}
		res |= nrzi_state ? m : 0;
	}
#endif

	nrzi->nrzi_state = nrzi_state;

	return res;
}

