#include <stdio.h>
#include "libdvd.h"
#include <stdlib.h>
#include <unistd.h>
#include <fec.h>


int main(void)
{
	efmplus_global_init();
	recordframe_global_init();
	eccblock_global_init();

	struct sync_context s;
	sync_init(&s);
	struct efmplus_context e;
	struct recordframe_context recordframe;
	struct eccblock_context eccblock;
	
	int last_efmplus_valid = 0;
	
	recordframe_init(&recordframe);
	eccblock_init(&eccblock);
	
	int c = 0;
	while (1)
	{
	
		int sync_status = 0;
		u16 sync_data = 0;
		
		//
		// Read data, sync
		//		
		{{
			u8 v;
			if (fread(&v, 1, 1, stdin) != 1)
				break;
			int i;
			for (i = 0; i < 8; ++i)
			{
				u16 data;
#ifdef LIBDVD_LSB_FIRST
				int r = sync_put(&s, v & (1<<i), &data);
#else
				int r = sync_put(&s, v & (0x80>>i), &data);
#endif
				if (r)
				{
					sync_status = r;
					sync_data = data;
				}
				c++;
			}
		}}
		
		if (sync_status & SYNC_RESYNC)
		{
			static int lc;
//			printf("S %04x %d\n", sync_data, c - lc);
			lc = c;
		}
		
		int efmplus_valid = 0;
		u8 efmplus_data;
		//
		// EFM+ decode
		//
		{{
			if (sync_status & SYNC_RESYNC)
			{
				if (last_efmplus_valid)
				{
					efmplus_decode(&e, &efmplus_data, sync_data);
					efmplus_valid = 1;
				}
			}	else if (sync_status & SYNC_DATA_FIRST)
			{
				efmplus_init_decode(&e, sync_data);
			} else if (sync_status & SYNC_DATA)
			{
				efmplus_decode(&e, &efmplus_data, sync_data);
				efmplus_valid = 1;
			}
			
			if (sync_status & SYNC_VALID)
				last_efmplus_valid = efmplus_valid;
		}}
		
//		printf("%04x %02x\n", sync_status, efmplus_valid ? efmplus_data : 0xcc);

		//
		// Recordframe
		//
		int recordframe_valid = 0;
		int eccblock_valid = 0;
		
		{{
		
			//
			// Note that last data byte is coincidental with next sync, so process it first.
			//
			if (efmplus_valid)
				recordframe_valid = recordframe_data(&recordframe, efmplus_data);

			if (recordframe_valid)
			{
				eccblock_valid = eccblock_set_recordframe(&eccblock, &recordframe);
			}

			// sync may re-set the pointer...
			if (sync_status & SYNC_RESYNC)
				recordframe_sync(&recordframe, (sync_identify((sync_data << 16) | 0x11) / 2) & 7);			
		}}

		
		//
		// ECC Block
		//
		{{
//			if (psn_valid)
//				printf("[[main::found recordframe %08x - %04x]]\n", psn, eccblock_done);

			if (eccblock_valid)
			{
				int pitotal, pototal, piuncorr, pouncorr;
				
				int eccblock_ecc_valid = eccblock_decode_ecc(&eccblock, &pitotal, &piuncorr, &pototal, &pouncorr);
				
				printf("[[main %08x: pitotal=%d (%du), pototal = %d (%du)]]\n", eccblock.psn, pitotal, piuncorr, pototal, pouncorr);
				
				if (eccblock_ecc_valid)
				{
					int i;
					for (i = 0; i < 16; ++i)
					{
						u8 userdata[2064];
						eccblock_get_userdata(&eccblock, userdata, i);
						
						userdata_scramble_dvd(userdata);
						
						u32 edc_difference = userdata_check_edc(userdata);
						
						if (!edc_difference)
							write(2, userdata, 2064);
						else
							printf("[[main: dropping psn %08x due to broken edc]]\n", eccblock.psn);
					}
				}
			}
		}}
	}

	return 0;
}
