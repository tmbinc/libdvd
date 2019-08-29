#include <stdio.h>
#include "libdvd.h"
#include <stdlib.h>

void fill_random(unsigned char *data, int len)
{
	while (len--)
		*data++ = rand();
}

int main(void)
{
	efmplus_global_init();

	unsigned char random_data[0x810], random_data_decoded[0x810];
	unsigned short random_data_efmp[0x810];
	
	fill_random(random_data, sizeof random_data);
	
	struct efmplus_context e;
	
	efmplus_init(&e);
	efmplus_encode_block(&e, random_data_efmp, random_data, sizeof random_data);
	efmplus_init(&e);
	efmplus_decode_block(&e, random_data_decoded, random_data_efmp, sizeof random_data);
	
	int i;
	for (i = 0; i < sizeof random_data; ++i)
		if (random_data[i] != random_data_decoded[i])
			printf("%04x: %02x -> %04x -> %02x\n", i, random_data[i], random_data_efmp[i], random_data_decoded[i]);
	return 0;
	
}
