#include <stdio.h>
#include <string.h>
#include "bootstrap.h"

// This must match bootsector.map
#define SECTORS_TO_COPY		0x40
#define DST_SEGMENT			0x42
#define INITIAL_IP			0x44
#define INITIAL_SP			0x46
#define INITIAL_CS			0x48
#define INITIAL_DS			0x4A
#define INITIAL_SS			0x4C

#define BYTES_PER_SECTOR	0x0B
#define SECTORS_PER_CLUSTER	0x0D
#define RESERVED_SECTORS	0x0E
#define N_FATS				0x10
#define MAX_ROOT_DIR_ENTRIES	0x11
#define TOTAL_LOGICAL_SECTORS	0x13
#define MEDIA_DESCRIPTOR		0x15
#define LOGICAL_SECTORS_PER_FAT	0x16
#define SECTORS_PER_TRACK	0x18
#define NUM_HEADS	0x1A
#define NUM_HIDDEN_SECTORS	0x1C

static unsigned char bootstrap_buf[513];

#define SET_16LE(off, val)	do { bootstrap_buf[off] = val; bootstrap_buf[off+1] = val >> 8; } while(0)
#define SET_32LE(off, val)	do { bootstrap_buf[off] = val; bootstrap_buf[off+1] = val >> 8; bootstrap_buf[off+2] = val >> 16; bootstrap_buf[off+3] = val >> 24; } while(0)

int bootstrap_write(unsigned short num_sectors_to_copy,
			unsigned short dst_segment,
			unsigned short initial_ip,
			unsigned short initial_sp,
			unsigned short initial_cs,
			unsigned short initial_ds,
			unsigned short initial_ss,
			struct bootstrap_bpb *bpb,
			const char *label,
			FILE *out_fptr)
{
	char label_buf[12];
	int label_len;

	memset(label_buf, ' ', sizeof(label_buf));
	label_len = strlen(label);
	if (label_len > 11) {
		fprintf(stderr, "Disk label '%s' is too long (max 11 chars.).\n", label);
		return -1;
	}

	bootstrap_buf[SECTORS_TO_COPY] = num_sectors_to_copy & 0xff;
	bootstrap_buf[SECTORS_TO_COPY+1] = num_sectors_to_copy >> 8;

	if (bpb) {
		SET_16LE(BYTES_PER_SECTOR, bpb->bytes_per_logical_sector);
		bootstrap_buf[SECTORS_PER_CLUSTER] = bpb->sectors_per_cluster;
		SET_16LE(RESERVED_SECTORS, bpb->reserved_logical_sectors);
		bootstrap_buf[N_FATS] = bpb->n_fats;
		SET_16LE(MAX_ROOT_DIR_ENTRIES, bpb->root_dir_entries);
		SET_16LE(TOTAL_LOGICAL_SECTORS, bpb->total_logical_sectors);
		bootstrap_buf[MEDIA_DESCRIPTOR] = bpb->media_descriptor;
		SET_16LE(LOGICAL_SECTORS_PER_FAT, bpb->logical_sectors_per_fat);

		bootstrap_buf[SECTORS_PER_TRACK] = bpb->sectors_per_track;
		SET_16LE(NUM_HEADS, bpb->num_heads);
		SET_32LE(NUM_HIDDEN_SECTORS, 0);
		bootstrap_buf[0x26] = 0x29; // EBPB signature
		memcpy(bootstrap_buf + 0x2b, label_buf, sizeof(label_buf));
		memcpy(bootstrap_buf + 0x36, "FAT12   ", 8);
	}

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

