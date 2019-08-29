#include <strings.h>
#include "libdvd.h"
#include <fec.h>
#include <stdio.h>
#include <string.h>

void eccblock_generate_pi(u8 *frame);
void eccblock_generate_po(u8 *frame);

static void *rs_pi, *rs_po, *rs_id;

void eccblock_init(struct eccblock_context *eccblock)
{
	eccblock->eccblock_done = 0;
	eccblock->psn_valid = 0;
}

static unsigned char lfsr_table[32767];
static int lfsr_val;

static int get_lfsr_byte(void)
{
	int i;
	int ret = lfsr_val;
	for (i=0; i<8; ++i)
	{
		int feed = (!(lfsr_val & 0x400)) ^ (!(lfsr_val & 0x4000));
		lfsr_val <<= 1;
		lfsr_val  |= feed;
		lfsr_val  &= 0x7FFF;
	}
	return ret;
}

		/* EDC, taken from rb's original gc seed calculation tool */
#define SHORTPOLY     0x80000011  // x^32 + x^31 + x^4 + 1

static u32 edc_table[256];

static void precalc_edc_table(void)
{
	int i,j;
	u32 entry;

	for(j=0;j<0x100;j++)
	{
		entry=j<<24;
		for(i=0;i<8;i++)
		{
			if(entry&0x80000000) { entry = entry << 1; entry=entry^SHORTPOLY; }
			else entry=entry << 1;
		}
		edc_table[j]=entry;
	}
}

u32 calculate_edc(unsigned char *buffer, u32 size)
{
	u32 idx=0;
	u64 edc=0;

	if(size<4) return(0);

		// preload register
	edc = edc | *(buffer++); edc = edc << 8;
	edc = edc | *(buffer++); edc = edc << 8;
	edc = edc | *(buffer++); edc = edc << 8;
	edc = edc | *(buffer++);

		// process all input bytes
	while(idx++<size)
	{
		edc = edc << 8;
		if(idx<size-3) edc = edc | *(buffer++);
		edc = edc ^ edc_table[(edc>>32)&0xFF];
	}

	return( (u32) edc );
}

void eccblock_global_init(void)
{
	lfsr_val = 1;
	int i;
	for (i = 0; i < 32767; ++i)
	{
		lfsr_table[i] = get_lfsr_byte();
	}

#define ID_N 6
#define ID_K 4
	rs_id = init_rs_char(8, 0x11d, 0, 1, ID_N - ID_K, 255 - ID_N);
#define PI_N 182
#define PI_K 172
	rs_pi = init_rs_char(8, 0x11d, 0, 1, PI_N - PI_K, 255 - PI_N);
#define PO_N 208
#define PO_K 192
	rs_po = init_rs_char(8, 0x11d, 0, 1, PO_N - PO_K, 255 - PO_N);

	precalc_edc_table();
}

int eccblock_set_recordframe(struct eccblock_context *eccblock, struct recordframe_context *recordframe)
{
	if (!recordframe_get_psn(recordframe, &eccblock->psn))
	{
		eccblock->psn_valid = 1;
		int idx = eccblock->psn & 0xF;
		if (idx == 0)
			eccblock->eccblock_done = 0;
		recordframe_get(recordframe, eccblock->eccblock + idx * 12 * 182, eccblock->eccblock + (192 + idx) * 182);
		eccblock->eccblock_done |= 1<<idx;
	} else
		eccblock->psn_valid = 0;
	
	return eccblock->eccblock_done == 0xFFFF;
}

void eccblock_get_recordframe(struct eccblock_context *eccblock, struct recordframe_context *recordframe, int index)
{
	recordframe_set(recordframe, eccblock->eccblock + index * 12 * 182, eccblock->eccblock + (192 + index) * 182);
}

int eccblock_decode_ecc(struct eccblock_context *eccblock, int *pitotal, int *piuncorr, int *pototal, int *pouncorr)
{
	int _pitotal, _pototal, _piuncorr, _pouncorr;
	
	if (!pitotal) pitotal = &_pitotal;
	if (!pototal) pototal = &_pototal;
	if (!piuncorr) piuncorr = &_piuncorr;
	if (!pouncorr) pouncorr = &_pouncorr;
	
	(*pitotal) = (*pototal) = (*piuncorr) = (*pouncorr) = 0;
	int i;

		/* handle PI */
	for (i=0; i<192 + 16; ++i)
	{
		u8 *d = eccblock->eccblock + i * 182;
		int nr_err = decode_rs_char(rs_pi, d, 0, 0);
		if (nr_err >= 0)
			(*pitotal) += nr_err;
		else
			(*piuncorr)++;
	}

		/* handle PO */
	for (i = 0; i < 172; ++i)
	{
		unsigned char col[192 + 16];
		int a;
		for (a = 0; a < 192 + 16; ++a)
			col[a] = eccblock->eccblock[i + a * 182];
		int nr_err = decode_rs_char(rs_po, col, 0, 0);

		if (nr_err >= 0)
			(*pototal) += nr_err;
		else
			(*pouncorr)++;

		for (a = 0; a < 192 + 16; ++a)
			eccblock->eccblock[i + a * 182] = col[a];
	}
	
	return !(*piuncorr || *pouncorr);
}

void eccblock_encode_ecc(struct eccblock_context *eccblock)
{
	int i;

		/* handle PO */
	for (i = 0; i < 172; ++i)
	{
		unsigned char col[192 + 16];
		int a;
		for (a = 0; a < 192 + 16; ++a)
			col[a] = eccblock->eccblock[i + a * 182];
		encode_rs_char(rs_po, col, col + 192);

		for (a = 0; a < 192 + 16; ++a)
			eccblock->eccblock[i + a * 182] = col[a];
	}
	
		/* handle PI */
	for (i=0; i<192 + 16; ++i)
	{
		u8 *d = eccblock->eccblock + i * 182;
		encode_rs_char(rs_pi, d, d + 172);
	}
}

void eccblock_get_userdata(struct eccblock_context *eccblock, u8 *userdata, int index)
{
	int row;
	
	for (row = 0; row < 12; ++row)
	{
		memcpy(userdata + row * 172, eccblock->eccblock + (index * 12 + row) * 182, 172);
	}
}

int eccblock_set_userdata(struct eccblock_context *eccblock, u8 *userdata)
{
	int row;
	
	int userdata_psn;
	
	userdata_psn  = userdata[0] << 24;
	userdata_psn |= userdata[1] << 16;
	userdata_psn |= userdata[2] <<  8;
	userdata_psn |= userdata[3] <<  0;
	
	if ((userdata_psn ^ eccblock->psn) & ~0xF)
	{
		eccblock->psn = userdata_psn;
		eccblock->eccblock_done = 0;
	}
	
	int index = userdata_psn & 0xF;
	
	for (row = 0; row < 12; ++row)
	{
		memcpy(eccblock->eccblock + (index * 12 + row) * 182, userdata + row * 172, 172);
	}

	eccblock->eccblock_done |= 1<<index;

	return eccblock->eccblock_done == 0xFFFF;
}

void userdata_scramble(u8 *userdata, int offset)
{
	int i;
	for (i=0; i<2048; ++i)
		userdata[i + 12] ^= lfsr_table[(offset + i) % 32767];
}

void userdata_scramble_dvd(u8 *userdata)
{
	userdata_scramble(userdata, ((userdata[3] >> 4) & 0xF) * 0x800);
}

u32 userdata_calc_edc(u8 *userdata)
{
	return calculate_edc(userdata, 2060);
}

void userdata_set_id(u8 *userdata, u32 id)
{
	userdata[0] = (id >> 24) & 0xFF;
	userdata[1] = (id >> 16) & 0xFF;
	userdata[2] = (id >>  8) & 0xFF;
	userdata[3] = (id >>  0) & 0xFF;
	
	encode_rs_char(rs_id, userdata, userdata+4);
}

u32 userdata_check_edc(u8 *userdata)
{
	u32 edc;
	
	edc  = userdata[2060] << 24;
	edc |= userdata[2061] << 16;
	edc |= userdata[2062] <<  8;
	edc |= userdata[2063] <<  0;
	
	return edc ^ userdata_calc_edc(userdata);
}

void userdata_set_edc(u8 *userdata)
{
	u32 edc = userdata_calc_edc(userdata);
	
	userdata[2060] = (edc >> 24) & 0xFF;
	userdata[2061] = (edc >> 16) & 0xFF;
	userdata[2062] = (edc >>  8) & 0xFF;
	userdata[2063] = (edc >>  0) & 0xFF;
}
