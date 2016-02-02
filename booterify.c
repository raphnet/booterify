#include <stdio.h>
#include <string.h>

#define NUM_SECTORS	5
#define SECTORS_PER_TRACK	6

unsigned char payload[0x10000];

int main(int argc, char **argv)
{
	FILE *fptr_bootstrap, *fptr_payload, *fptr_disk;
	unsigned char bootstrap_buf[513];
	int bootstrap_size;
	int payload_size;
	unsigned short n_sectors;
	unsigned char sectors_per_track = 9;
	unsigned int disk_image_size = 360*1024;

	if (argc < 3) {
		printf("Usage: ./booterify bootsector.bin file.com output.dsk\n");
		return 0;
	}

	fptr_bootstrap = fopen(argv[1], "rb");
	if (!fptr_bootstrap) {
		perror("could not open bootstrap");
		return -1;
	}

	fptr_payload = fopen(argv[2], "rb");
	if (!fptr_payload) {
		perror("could not open payload");
		fclose(fptr_bootstrap);
		return -1;
	}

	memset(bootstrap_buf, 0, sizeof(bootstrap_buf));
	bootstrap_size =  fread(bootstrap_buf, 1, 513, fptr_bootstrap);
	if (bootstrap_size < 0) {
		perror("fread");
		goto err;
	}
	if (bootstrap_size > 512) {
		fprintf(stderr, "Bootstrap too large");
		goto err;
	}

	printf("Bootstrap size: %d\n", bootstrap_size);

	payload_size =  fread(payload, 1, 0x10001, fptr_payload);
	if (payload_size < 0) {
		perror("fread");
		goto err;
	}
	if (payload_size > 0x10000) {
		fprintf(stderr, "Payload too large");
		goto err;
	}

	printf("Payload size: %d\n", payload_size);

	/** Configure the bootloader */
	// 1 : Number of sectors to copy
	if (payload_size < 512) {
		n_sectors = 1;
	} else {
		n_sectors = payload_size / 512;
	}
	payload[NUM_SECTORS] = n_sectors & 0xff;
	payload[NUM_SECTORS+1] = n_sectors >> 8;

	// 2: Sectors per track
	payload[SECTORS_PER_TRACK] = sectors_per_track;


	/** Write the file */
	fptr_disk = fopen(argv[3], "wb");
	if (!fptr_disk) {
		perror("fopen");
		goto err;
	}

	fwrite(bootstrap_buf, 512, 1, fptr_disk);
	// our input is a .com file starting at 100H. On disk, we write 0x100 zeros.
	fseek(fptr_disk, 0x100, SEEK_CUR);
	fwrite(payload, payload_size, 1, fptr_disk);
	fseek(fptr_disk, disk_image_size-1, SEEK_SET);
	// Pad file size
	bootstrap_buf[512] = 0;
	fwrite(bootstrap_buf, 1, 1, fptr_disk);

err:
	fclose(fptr_bootstrap);
	fclose(fptr_payload);

	return 0;
}
