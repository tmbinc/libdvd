#include <stdio.h>
#include <stdlib.h>
#include "libdvd.h"
#include "efmplus_table.h"

		/* EFMplus (simplified) decode table */
#define EFMP_INVALID 0x100
static int efmp_decode[65536][2]; // plain | (invalid << 8)

void efmplus_global_init(void)
{
	int i, s;
	for (i=0; i<65536; ++i)
	{
		efmp_decode[i][0] = EFMP_INVALID;
		efmp_decode[i][1] = EFMP_INVALID;
	}

	for (i=0; i<256 + 88; ++i)
		for (s=0; s<4; ++s)
		{
			int entry = efmp_cw[i][s];
			int ns = efmp_ns[i][s];
			int a = ns == 1; // next_state_is_2

			if ((efmp_decode[entry][a] != EFMP_INVALID) && ((efmp_decode[entry][a]&0xFF) != (i&0xFF)))
			{
				printf("[EFM+] ERROR: %02x:%d is already used (by %02x:%d)\n", i, s, efmp_decode[entry][a] & 0xFF, (efmp_decode[entry][a] >> 8) & 3);
				exit(0);
			}

			efmp_decode[entry][a] = i & 0xFF;

			if ((ns != 1) && (ns != 2))
			{
				if ((efmp_decode[entry][!a] != EFMP_INVALID) && ((efmp_decode[entry][!a]&0xFF) != (i&0xFF)))
				{
					printf("[EFM+] ERROR: alt %02x:%d is already used (by %02x:%d)\n", i, s, efmp_decode[entry][a] & 0xFF, (efmp_decode[entry][a] >> 8) & 3);
					exit(0);
				}
				efmp_decode[entry][!a] = i & 0xFF;
			}

//			printf("[EFM+] %d:%04x:%d = %04x\n", s, entry, a, i | (ns<<8));
		}
}

void efmplus_init(struct efmplus_context *c)
{
	c->last = 0;
	c->state = 0;
	nrzi_init(&c->nrzi);
}

void efmplus_init_decode(struct efmplus_context *c, u16 first_cw)
{
	efmplus_init(c);
	c->last = first_cw;
}

int efmplus_decode(struct efmplus_context *c, u8 *res, u16 codeword)
{
	int next_state_is_2 = (codeword & 0x8008) == 0;

	int decode_result = efmp_decode[c->last][next_state_is_2];

	c->last = codeword;
	
	*res = decode_result & 0xFF;
	
	if (decode_result & EFMP_INVALID)
		return -1;

	return 0;
}

int efmplus_decode_block(struct efmplus_context *c, u8 *res, u16 *codewords, int len)
{
	int wr = 0;
	
	efmplus_init_decode(c, *codewords++);
	
	while (len--)
	{
		if (efmplus_decode(c, res++, len ? *codewords++ : 0))
			break;

		wr++;
	}
	if (len >= 0)
		printf("[efm+ decode failed at %d (%d)]\n", wr, len);
	return wr;
}

int efmplus_encode(struct efmplus_context *c, u8 data, u16 *codeword, int alt)
{
	int i = data;
	
	int res = 0;
	
	// For the 8-bit bytes in the range 0 to 87, the Substitution table offers an alternative 16-bit Code Word for all States
#if 1
	if (i < 88)
	{
		if (alt == -1)
			res = 1;
		else if (alt == 1)
			i += 256;
	} else 
#endif
#if 0
	if ((c->state == 0) || (c->state == 3))
	{
		if (alt == -1)
			res = 1;
		else if (alt == 1)
		{
			c->state ^= 3;
		}
	} else
#endif

	{
		if (alt != -1)
		{
			printf("[efm+ encode: asked for alt %d but %d state %d]\n", alt, i, c->state);
		}
	}
	
	if (!res)
	{
		*codeword = nrzi_encode(&c->nrzi, efmp_cw[i][c->state]);
		c->state = efmp_ns[i][c->state];
	}
	
	return res;
}
