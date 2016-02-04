#ifndef _bootstrap_h__
#define _bootstrap_h__

int bootstrap_load(const char *filename);
int bootstrap_write(unsigned short num_sectors_to_copy, unsigned char sectors_per_track,
			unsigned short initial_ip,
			unsigned short initial_sp,
			unsigned short initial_cs,
			unsigned short initial_ds,
			unsigned short initial_ss,
			FILE *out_fptr);

#endif // _bootstrap_h__
