#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libdvd.h"

int main(void)
{
	struct eccblock_context eccblock;
	struct recordframe_context recordframe;
	
	eccblock_global_init();
	
	eccblock_init(&eccblock);
	recordframe_init(&recordframe);
	
	struct efmplus_context efm;
	efmplus_init(&efm);
	
	while (1)
	{
		int eccblock_done = 0;
		
		{{ // userdata -> eccblock
			u8 sector[2064];
			if (!fread(sector, 2064, 1, stdin))
				break;
			printf("reading block %02x %02x %02x %02x\n", sector[0], sector[1], sector[2], sector[3]);
			
			userdata_scramble(sector, ((sector[3]>>4) & 0xF) * 0x800);
			
			eccblock_done = eccblock_set_userdata(&eccblock, sector);
		}}
		
		{{ // ecc generation
			if (eccblock_done)
			{
				printf("eccblock done\n");
				eccblock_encode_ecc(&eccblock);
//				fwrite(eccblock.eccblock, 208, 182, stderr);
			}
		}}

		{{
			if (eccblock_done)
			{
				printf("eccblock done\n");
				int i, j;
				for (i = 0; i < 16; ++i)
				{
					eccblock_get_recordframe(&eccblock, &recordframe, i);
//					fwrite(recordframe.recordframe, 13, 182, stderr);
					u16 data[93*26];
					recordframe_generate(&recordframe, &efm, data);

#ifdef LIBDVD_LSB_FIRST
					for (j = 0; j < 93 * 26; ++j)
					{
						u8 d[2];
						d[0] = (data[j] >> 0) & 0xFF;
						d[1] = (data[j] >> 8) & 0xFF;
						fwrite(d, 2, 1, stderr);
					}
#else
					for (j = 0; j < 93 * 26; ++j)
					{
						u8 d[2];
						d[0] = (data[j] >> 8) & 0xFF;
						d[1] = (data[j] >> 0) & 0xFF;
						fwrite(d, 2, 1, stderr);
					}
#endif
				}
			}
		}}
		
	}
	
	return 0;
}
