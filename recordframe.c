#include <stdio.h>
#include <fec.h>
#include <string.h>
#include <stdlib.h>
#include "libdvd.h"

static void *rs_id;

void recordframe_global_init(void)
{
#define ID_N 6
#define ID_K 4
	rs_id = init_rs_char(8, 0x11d, 0, 1, ID_N - ID_K, 255 - ID_N);
}

void recordframe_init(struct recordframe_context *c)
{
	c->current_syncframe = -1;
}

int  recordframe_sync(struct recordframe_context *c, int sy)
{
	const int sync_frame_ids[26] = {0, 5, 1, 5, 2, 5, 3, 5, 4, 5, 1, 6, 2, 6, 3, 6, 4, 6, 1, 7, 2, 7, 3, 7, 4, 7};
	c->last_SY[0] = c->last_SY[1];
	c->last_SY[1] = sy;
				
	if (c->sync_distance != 91)
	{
		printf("[[recordframe::incomplete %d]]\n", c->sync_distance);
	}
	c->sync_distance = 0;
	
//	printf("[[recordframe::last_sy]] %d -> %d\n", c->last_SY[0], c->last_SY[1]);

	if (sy == 0)
	{
		if (c->current_syncframe != -1 && c->current_syncframe != 25)
			printf("[[recordframe::abort %d]]\n", c->current_syncframe);
		c->current_syncframe = 0;
	} else if (c->current_syncframe != -1)
		c->current_syncframe++;
		
	if (c->current_syncframe < 0 || c->current_syncframe >= 26 || sync_frame_ids[c->current_syncframe] != sy)
	{
		printf("[[[INVALID SYNC %d -> %d at syncframe %d]]]\n", c->last_SY[0], c->last_SY[1], c->current_syncframe);
		c->current_syncframe = -1;
	}

	if (c->current_syncframe == 0)
		c->pointer = 0;

	return 0;
}

int  recordframe_data(struct recordframe_context *c, u8 data)
{
	if (c->sync_distance != 91)
		c->sync_distance++;
	else
	{
		printf("[[recordframe:drop]]\n");
		return 0;
	}

	if (c->current_syncframe != -1)
		c->recordframe[c->pointer++] = data;
		
	if (c->pointer == 13 * 182)
		return 1;
	
	return 0;
}

int  recordframe_get(struct recordframe_context *c, u8 *userdata, u8 *eccdata)
{
	int row;
	if (c->pointer != 13 * 182)
		return -1;
	for (row = 0; row < 13; ++row)
	{
		if (row == 12)
			userdata = eccdata;

		memcpy(userdata, c->recordframe + row * 182, 182);
		userdata += 182;
	}
	return 0;
}

void recordframe_set(struct recordframe_context *c, u8 *userdata, u8 *eccdata)
{
	int row;
	for (row = 0; row < 13; ++row)
	{
		if (row == 12)
			userdata = eccdata;

		memcpy(c->recordframe + row * 182, userdata, 182);
		userdata += 182;
	}
	c->pointer = 13 * 182;
}

int  recordframe_get_psn(struct recordframe_context *c, u32 *psn)
{
	unsigned char *userdata = c->recordframe; 
	int errors_in_id = decode_rs_char(rs_id, userdata, 0, 0);
	if (errors_in_id)
	{
		printf("[[recordframe::ied error]]\n");
//		return -1;
	}
	*psn = (userdata[0] << 24) | (userdata[1] << 16) | (userdata[2] << 8)  | userdata[3];
	return 0;
}

int calc_dsv(u16 data)
{
	int dsv = 0;
	int i;
	for (i = 0; i < 16; ++i)
		dsv += (data & (1<<i)) ? 1 : -1;
	return dsv;
}

void recordframe_generate(struct recordframe_context *c, struct efmplus_context *c_efm, u16 *frame)
{
	int row;
	
	const int sync_pattern[13 * 2] = 
		{0, 5, 1, 5, 2, 5, 3, 5, 4, 5, 1, 6, 2, 6, 3, 6, 4, 6, 1, 7, 2, 7, 3, 7, 4, 7};
	
	u8 *data = c->recordframe;
	
	for (row = 0; row < 13 * 2; ++row)
	{
		u16 altstream[93];
		struct efmplus_context altefm;
		u16 *stream[2] = {frame, altstream};
		struct efmplus_context *efm[2] = {c_efm, &altefm};
		int dsv[2];
		
		altefm = *c_efm;
	
		int alt = 0;
		
		for (alt = 0; alt < 2; ++alt)
		{
			int state = efm[alt]->state >> 1;
			u32 sync = sync_get(sync_pattern[row], state, alt);
			dsv[alt]  = calc_dsv(stream[alt][0] = nrzi_encode(&efm[alt]->nrzi, (sync >> 16) & 0xFFFF));
			dsv[alt] += calc_dsv(stream[alt][1] = nrzi_encode(&efm[alt]->nrzi, (sync >>  0) & 0xFFFF));
			efm[alt]->state = 0;
		}
		
		int b;
		
		for (b = 0; b < 91; ++b)
		{
			int decision[2];
			
			int bdsv[2];
			
			for (alt = 0; alt < 2; ++alt)
			{
				decision[alt] = efmplus_encode(efm[alt], data[b], &stream[alt][b+2], -1);
				
				// The DSV of each stream is computed up to the 8-bit byte preceding
				// the 8-bit byte for which there is this choice. 
				bdsv[alt] = calc_dsv(stream[alt][b+2]);
			}
			
//			printf("decision %d, %d (dsv %d, %d)\n", decision[0], decision[1], dsv[0], dsv[1]);
			
			if (decision[0] && decision[1])
			{
				// The stream with the lowest |DSV| is selected and duplicated to the other stream.
				int winner = abs(dsv[0]) < abs(dsv[1]) ? 0 : 1;
#if 1
//				printf("winner is %d\n", winner);
				memcpy(stream[winner ^ 1], stream[winner], (b + 2) * sizeof(u16));
				dsv[winner ^ 1] = dsv[winner];
				(*efm[winner ^ 1]) = (*efm[winner]);
#endif
				// Then, one of the representations of the next 8-bit byte is entered into Stream 1 and the other into Stream 2. 
				for (alt = 0; alt < 2; ++alt)
				{
					efmplus_encode(efm[alt], data[b], &stream[alt][b+2], alt);
					bdsv[alt] = calc_dsv(stream[alt][b+2]);
				}
			} else if (decision[0] || decision[1])
			{
				int cstream = decision[0] ? 0 : 1;

//				printf("decision stream is %d\n", cstream);

#if 1
				if (abs(dsv[cstream]) < abs(dsv[cstream ^ 1]))
				{
					// If the |DSV| of the stream in which case c) occurs is smaller than that of the other 
					// stream, then the stream in which case c) has occurred is chosen and duplicated to the other stream. 
					memcpy(stream[cstream ^ 1], stream[cstream], (b + 2) * sizeof(u16));
					dsv[cstream ^ 1] = dsv[cstream];
					*efm[cstream ^ 1] = *efm[cstream];

					// One of the representations of the next 8- bit byte is entered into this stream and the other into the other stream.
					for (alt = 0; alt < 2; ++alt)
					{
						efmplus_encode(efm[alt], data[b], &stream[alt][b+2], alt);
						bdsv[alt] = calc_dsv(stream[alt][b+2]);
					}
				} else
#endif
				{
					// If the |DSV| of the stream in which case c) has occurred is larger than 
					// that of the other stream, then case c) is ignored and the 8-bit byte is 
					// represented according to the prescribed State.
					efmplus_encode(efm[cstream], data[b], &stream[cstream][b+2], 0);
					bdsv[cstream] = calc_dsv(stream[cstream][b+2]);
				}
			}
			
			for (alt = 0; alt < 2; ++alt)
				dsv[alt] += bdsv[alt];
		}
		
//		printf("final DSV values; %d %d\n", dsv[0], dsv[1]);

		frame += 93;
		data += 91;
	}
}
