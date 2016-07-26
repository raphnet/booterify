#include <stdio.h>
#include <string.h>
#include "bootstrap.h"

// This must match bootsector.map
#define NUM_SECTORS			0x40
#define SECTORS_PER_TRACK	0x18
#define DST_SEGMENT			0x42
#define INITIAL_IP			0x44
#define INITIAL_SP			0x46
#define INITIAL_CS			0x48
#define INITIAL_DS			0x4A
#define INITIAL_SS			0x4C

static unsigned char bootstrap_buf[513];

int bootstrap_write(unsigned short num_sectors_to_copy, unsigned char sectors_per_track,
			unsigned short dst_segment,
			unsigned short initial_ip,
			unsigned short initial_sp,
			unsigned short initial_cs,
			unsigned short initial_ds,
			unsigned short initial_ss,
			FILE *out_fptr)
{
	bootstrap_buf[NUM_SECTORS] = num_sectors_to_copy & 0xff;
	bootstrap_buf[NUM_SECTORS+1] = num_sectors_to_copy >> 8;

	bootstrap_buf[SECTORS_PER_TRACK] = sectors_per_track;

	bootstrap_buf[INITIAL_IP] = initial_ip & 0xff;
	bootstrap_buf[INITIAL_IP+1] = initial_ip >> 8;

	bootstrap_buf[INITIAL_SP] = initial_sp & 0xff;
	bootstrap_buf[INITIAL_SP+1] = initial_sp >> 8;
	bootstrap_buf[INITIAL_CS] = initial_cs & 0xff;
	bootstrap_buf[INITIAL_CS+1] = initial_cs >> 8;

	bootstrap_buf[INITIAL_DS] = initial_ds & 0xff;
	bootstrap_buf[INITIAL_DS+1] = initial_ds >> 8;

	bootstrap_buf[INITIAL_SS] = initial_ss & 0xff;
	bootstrap_buf[INITIAL_SS+1] = initial_ss >> 8;

	return fwrite(bootstrap_buf, 512, 1, out_fptr);
}

int bootstrap_load(const char *filename)
{
	FILE *fptr_bootstrap;
	int bootstrap_size;

	fptr_bootstrap = fopen(filename, "rb");
	if (!fptr_bootstrap) {
		perror("could not open bootstrap");
		return -1;
	}

	memset(bootstrap_buf, 0, sizeof(bootstrap_buf));
	bootstrap_size =  fread(bootstrap_buf, 1, 513, fptr_bootstrap);
	if (bootstrap_size < 0) {
		perror("fread");
		fclose(fptr_bootstrap);
		return -1;
	}
	if (bootstrap_size > 512) {
		fprintf(stderr, "Bootstrap too large");
		fclose(fptr_bootstrap);
		return -1;
	}

	fclose(fptr_bootstrap);

	return bootstrap_size;
}

