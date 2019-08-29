#ifndef __LIBDVD_H
#define __LIBDVD_H

#define LIBDVD_LSB_FIRST

typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct nrzi_context
{
	int nrzi_state;
	int dist;
};

void nrzi_init(struct nrzi_context *nrzi);
u16 nrzi_encode(struct nrzi_context *nrzi, u16 nrz);

struct efmplus_context
{
	u16 last;
	int state;
	struct nrzi_context nrzi;
};

void efmplus_global_init(void);
void efmplus_init(struct efmplus_context *c);
void efmplus_init_decode(struct efmplus_context *c, u16 first_cw);
int efmplus_decode(struct efmplus_context *c, u8 *res, u16 codeword);
int efmplus_decode_block(struct efmplus_context *c, u8 *res, u16 *codewords, int len);
int efmplus_encode(struct efmplus_context *c, u8 data, u16 *codeword, int alt);

struct sync_context
{
	u32 data;
	int b, last;
};

void sync_init(struct sync_context *c);
int sync_identify(u32 code);
#define SYNC_VALID      1
#define SYNC_DATA       2
#define SYNC_DATA_FIRST 4
#define SYNC_RESYNC     8

int sync_put(struct sync_context *c, int bit, u16 *data);
u32 sync_get(int sy, int state, int alt);

struct recordframe_context
{
	int last_SY[2];
	int current_syncframe;
	int pointer;
	
	int sync_distance;
	
	u8 recordframe[13 * 182];
};

void recordframe_global_init(void);
void recordframe_init(struct recordframe_context *c);
int  recordframe_sync(struct recordframe_context *c, int sy);
int  recordframe_data(struct recordframe_context *c, u8 data);
int  recordframe_get(struct recordframe_context *c, u8* userdata, u8 *eccdata);
void recordframe_set(struct recordframe_context *c, u8* userdata, u8 *eccdata);
int  recordframe_get_psn(struct recordframe_context *c, u32 *psn);
void recordframe_generate(struct recordframe_context *c, struct efmplus_context *c_efm, u16 *frame);

struct eccblock_context
{
	u8 eccblock[208 * 182];
	int eccblock_done;
	u32 psn;
	int psn_valid;
};

void eccblock_global_init(void);
void eccblock_init(struct eccblock_context *eccblock);
int eccblock_set_recordframe(struct eccblock_context *eccblock, struct recordframe_context *recordframe);
void eccblock_get_recordframe(struct eccblock_context *eccblock, struct recordframe_context *recordframe, int index);
int eccblock_decode_ecc(struct eccblock_context *eccblock, int *pitotal, int *piuncorr, int *pototal, int *pouncorr);
void eccblock_encode_ecc(struct eccblock_context *eccblock);

void eccblock_get_userdata(struct eccblock_context *eccblock, u8 *userdata, int index);
int eccblock_set_userdata(struct eccblock_context *eccblock, u8 *userdata);

void userdata_scramble(u8 *userdata, int seed_offset);
void userdata_scramble_dvd(u8 *userdata);
u32 userdata_calc_edc(u8 *userdata);
u32 userdata_check_edc(u8 *userdata);
void userdata_set_edc(u8 *userdata);
void userdata_set_id(u8 *userdata, u32 id);


#endif
