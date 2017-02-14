#ifndef _bootstrap_h__
#define _bootstrap_h__

#include <stdint.h>

struct bootstrap_bpb {
		/* DOS 2.0 BPB */
		uint16_t bytes_per_logical_sector;
		uint8_t sectors_per_cluster;
		uint16_t reserved_logical_sectors;
		uint8_t n_fats;
		uint16_t root_dir_entries;
		uint16_t total_logical_sectors;
		uint8_t media_descriptor;
		uint16_t logical_sectors_per_fat;

		/* DOS 3.0 BPB */
		uint16_t sectors_per_track;
		uint8_t num_heads;

		// Internal stuff
		uint32_t disk_image_size;
};

int bootstrap_load(const char *filename);
int bootstrap_write(unsigned short num_sectors_to_copy,
			unsigned short dst_segment,
			unsigned short initial_ip,
			unsigned short initial_sp,
			unsigned short initial_cs,
			unsigned short initial_ds,
			unsigned short initial_ss,
			struct bootstrap_bpb *bpb,
			const char *label,
			FILE *out_fptr);

#endif // _bootstrap_h__
