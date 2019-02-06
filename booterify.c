#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include "bootstrap.h"

#define DESTINATION_SEGMENT	0x1000

#define DEFAULT_DISK_LABEL	"Booterify"

unsigned char payload[0x10000];

// EXE header fields
#define SIG					0
#define LAST_PAGE_SIZE		1
#define FILE_PAGES			2
#define RELOCATION_ITEMS	3
#define HEADER_PARAGRAPHS	4
#define MINALLOC			5
#define MAXALLOC			6
#define INITIAL_SS			7
#define INITIAL_SP			8
#define CHECKSUM			9
#define INITIAL_IP			10
#define PRE_RELOCATE_CS		11
#define RELOCATION_TABLE_OFFSET	12
#define OVERLAY_NUMBER		13

void hexdump(const unsigned char *data, int len, int address)
{
	int i;

	for (i=0; i<len; i++) {
		if (!(i%8)) {
			if (i)
				printf("\n");
			printf("\t%04x : ", address + i);
		}
		printf("%02x ", *data);
		data++;
	}
	printf("\n");
}

static int read_uint16le(FILE *fptr, uint16_t *dst)
{
	unsigned char buf[2];

	if (1 != fread(buf, 2, 1, fptr)) {
		perror("fread");
		return -1;
	}

	*dst = buf[0] | (buf[1]<<8);

	return 0;
}

static void printHelp(void)
{
	printf("Usage: ./booterify [options] bootsector.bin executable output.dsk\n");
	printf("\n");
	printf("Where:\n");
	printf(" - bootsector.bin is the bootloader,\n");
	printf(" - executable is the input file (.exe or .com),\n");
	printf(" - output.dsk is the output file.\n");
	printf("\n");
	printf("Options:\n");
	printf(" -h             Prints help\n");
	//printf(" -c             Force input file format as .com (default: auto-detect)\n");
	//printf(" -e             Force input file format as .exe (default: auto-detect)\n");
	printf(" -f size        Floppy format. Supports 320, 360, 720, 1200 and 1440 sizes.\n");
	printf(" -l label       Set floppy label. (Default: '%s')\n", DEFAULT_DISK_LABEL);
	printf("\n");
	printf("Debugging/Development:\n");
	printf(" -B file        Generate list of breakpoints commands for bochs for all\n");
	printf("                instances of DOS int calls\n");
}

int getBPB_for_size(int f, struct bootstrap_bpb *bpb)
{
	struct bootstrap_bpb f320k_bpb = {
		.bytes_per_logical_sector = 512,
		.sectors_per_cluster = 2,
		.reserved_logical_sectors = 1,
		.n_fats = 2,
		.root_dir_entries = 112,
		.total_logical_sectors = 8 * 2 * 40, // sectors * heads * tracks
		.media_descriptor = 0xFF, // 320K
		.logical_sectors_per_fat = 1,
		.sectors_per_track = 8,
		.num_heads = 2,

		.disk_image_size = 320 * 1024,
	};
	struct bootstrap_bpb f360k_bpb = {
		.bytes_per_logical_sector = 512,
		.sectors_per_cluster = 2,
		.reserved_logical_sectors = 1,
		.n_fats = 2,
		.root_dir_entries = 112,
		.total_logical_sectors = 9 * 2 * 40, // sectors * heads * tracks
		.media_descriptor = 0xFD, // 360K
		.logical_sectors_per_fat = 2, // FAT occupies two sectors
		.sectors_per_track = 9,
		.num_heads = 2,

		.disk_image_size = 360 * 1024,
	};
	struct bootstrap_bpb f720k_bpb = {
		.bytes_per_logical_sector = 512,
		.sectors_per_cluster = 2,
		.reserved_logical_sectors = 1,
		.n_fats = 2,
		.root_dir_entries = 112,
		.total_logical_sectors = 9 * 2 * 80,
		.media_descriptor = 0xF9,
		.logical_sectors_per_fat = 3,
		.sectors_per_track = 9,
		.num_heads = 2,

		.disk_image_size = 720 * 1024,
	};
	struct bootstrap_bpb f1200k_bpb = {
		.bytes_per_logical_sector = 512,
		.sectors_per_cluster = 1,
		.reserved_logical_sectors = 1,
		.n_fats = 2,
		.root_dir_entries = 224,
		.total_logical_sectors = 15 * 2 * 80,
		.media_descriptor = 0xF9,
		.logical_sectors_per_fat = 7,
		.sectors_per_track = 15,
		.num_heads = 2,

		.disk_image_size = 1200 * 1024,
	};
	struct bootstrap_bpb f1440k_bpb = {
		.bytes_per_logical_sector = 512,
		.sectors_per_cluster = 1,
		.reserved_logical_sectors = 1,
		.n_fats = 2,
		.root_dir_entries = 224,
		.total_logical_sectors = 18 * 2 * 80, // sectors * heads * tracks
		.media_descriptor = 0xF0,
		.logical_sectors_per_fat = 9,
		.sectors_per_track = 18,
		.num_heads = 2,

		.disk_image_size = 1440 * 1024,
	};



	switch(f)
	{
		case 320: memcpy(bpb, &f320k_bpb, sizeof(struct bootstrap_bpb)); return 0;
		case 360: memcpy(bpb, &f360k_bpb, sizeof(struct bootstrap_bpb)); return 0;
		case 720: memcpy(bpb, &f720k_bpb, sizeof(struct bootstrap_bpb)); return 0;
		case 1200: memcpy(bpb, &f1200k_bpb, sizeof(struct bootstrap_bpb)); return 0;
		case 1440: memcpy(bpb, &f1440k_bpb, sizeof(struct bootstrap_bpb)); return 0;
	}

	return -1;
}

int writeVolumeLabel(FILE *outfptr, const char *name, uint8_t attr, uint16_t mod_time,
						uint16_t mod_date, uint16_t start_cluster, uint32_t file_size)
{
	uint8_t directory_entry[32];
	int namelength;

	memset(directory_entry, 0, sizeof(directory_entry));
	memset(directory_entry, ' ', 11);
	memcpy(directory_entry, name, 11);
	namelength = strlen(name);
	if (namelength > 11) {
		fprintf(stderr, "Volume label is too long\n");
		return -1;
	}
	memcpy(directory_entry, name, namelength);

	directory_entry[0x0B] = attr;
	directory_entry[0x16] = mod_time;
	directory_entry[0x17] = mod_time >> 8;
	directory_entry[0x18] = mod_date;
	directory_entry[0x19] = mod_date >> 8;
	directory_entry[0x1A] = start_cluster;
	directory_entry[0x1B] = start_cluster >> 8;
	directory_entry[0x1C] = file_size;
	directory_entry[0x1D] = file_size >> 8;
	directory_entry[0x1E] = file_size >> 16;
	directory_entry[0x1F] = file_size >> 24;

	fwrite(directory_entry, sizeof(directory_entry), 1, outfptr);
	return 0;
}

int writeDirectoryEntry(FILE *outfptr, const char *filename, const char *extension,
						uint8_t attr, uint16_t mod_time, uint16_t mod_date,
						uint16_t start_cluster, uint32_t file_size)
{
	char tmp[12];
	snprintf(tmp, 12, "%-8s%-3s", filename, extension);
	return writeVolumeLabel(outfptr, tmp, attr, mod_time, mod_date, start_cluster, file_size);
}

int main(int argc, char **argv)
{
	FILE *fptr_payload, *fptr_disk;
	int bootstrap_size;
	int payload_size;
	unsigned short n_sectors, initial_ip = 0x100;
	unsigned short initial_cs = DESTINATION_SEGMENT;
	unsigned short initial_ds = DESTINATION_SEGMENT;
	unsigned short initial_ss = DESTINATION_SEGMENT;
	unsigned short initial_sp = 0xFFFF;
	unsigned char sectors_per_track = 9;
	int opt, i, retval = -1;
	char *e;
	const char *bootstrap_file, *executable_file, *output_file;
	uint16_t exe_header[14];
	char is_exe;
	int dos_interrupts = 0;
	FILE *breakpoints_ints_fptr = NULL;
	struct bootstrap_bpb bpb;
	int first_cluster_sector = 512;
	int disk_size = 1440;
	const char *disk_label = DEFAULT_DISK_LABEL;


	while (-1 != (opt = getopt(argc, argv, "hB:f:l:"))) {
		switch(opt)
		{
			case 'h':
				printHelp();
				return 0;
				break;
			case '?':
				fprintf(stderr, "Unknown option. Try -h\n");
				return 1;
				break;
			case 'f':
				disk_size = atoi(optarg);
				break;
			case 'l':
				disk_label = optarg;
				break;
			case 'B':
				breakpoints_ints_fptr = fopen(optarg, "w");
				if (!breakpoints_ints_fptr) {
					perror("fopen breakpoint file");
					return 1;
				}
				break;
		}
	}

	if (argc - optind < 3) {
		fprintf(stderr, "Missing arguments\n");
		return 1;
	}

	if (getBPB_for_size(disk_size, &bpb)) {
		fprintf(stderr, "Invalid disk size\n");
		return -1;
	}

	bootstrap_file = argv[optind];
	executable_file = argv[optind+1];
	output_file = argv[optind+2];

	bootstrap_size = bootstrap_load(bootstrap_file);
	if (bootstrap_size < 0) {
		return -1;
	}

	fptr_payload = fopen(executable_file, "rb");
	if (!fptr_payload) {
		perror("could not open payload");
		return -1;
	}

	read_uint16le(fptr_payload, exe_header);
	fseek(fptr_payload, 0, SEEK_SET);

	if (exe_header[SIG] != 0x5a4d) {
		is_exe = 0;
	} else {
		is_exe = 1;
	}

	printf("Bootstrap file: %s\n", bootstrap_file);
	printf("Bootstrap size: %d\n", bootstrap_size);
	printf("Payload file: %s\n", executable_file);
	printf("Payload file type: %s\n", is_exe ? ".EXE" : ".COM");
	printf("Output file: %s\n", output_file);
	printf("Sectors per track: %d\n", sectors_per_track);
	printf("Output file padding: %d B / %.2f kB / %.2f MB)\n", bpb.disk_image_size, bpb.disk_image_size / 1024.0, bpb.disk_image_size / 1024.0 / 1024.0);

	if (is_exe) {
		int file_size;

		printf("Reading exe file...\n");
		for (i=0; i<14; i++) {
			if (read_uint16le(fptr_payload, &exe_header[i])) {
				goto err;
			}
		}

		file_size = exe_header[FILE_PAGES]*512;
		if (exe_header[LAST_PAGE_SIZE]) {
			file_size -= 512 - exe_header[LAST_PAGE_SIZE];
		}
		printf("File size: %d\n", file_size);

		payload_size = file_size - exe_header[HEADER_PARAGRAPHS] * 16;

		printf("Load module size: %d\n", payload_size);

		if (payload_size > 0x10000) {
			fprintf(stderr, "Payload too large");
			goto err;
		}

		fseek(fptr_payload, exe_header[HEADER_PARAGRAPHS] * 16, SEEK_SET);
		if (1 != fread(payload, payload_size, 1, fptr_payload)) {
			perror("error reading exe loadmodule\n");
			goto err;
		}

		initial_ip = exe_header[INITIAL_IP];
		initial_sp = exe_header[INITIAL_SP];

		printf("Relocating...\n");
		initial_cs = exe_header[PRE_RELOCATE_CS] + DESTINATION_SEGMENT;
		initial_ss = exe_header[INITIAL_SS] + DESTINATION_SEGMENT;

		fseek(fptr_payload, exe_header[RELOCATION_TABLE_OFFSET], SEEK_SET);
		for (i=0; i<exe_header[RELOCATION_ITEMS]; i++) {
			uint16_t offset, segment;
			uint32_t addr;
			uint16_t val;

			read_uint16le(fptr_payload, &offset);
			read_uint16le(fptr_payload, &segment);
			addr = segment * 16 + offset;
			printf("Relocation %d : %04x:%04x (linear address 0x%08x) : ", i, segment, offset, addr);
			if (addr >= 0x10000) {
				fprintf(stderr, "Relocation out of bounds\n");
				goto err;
			}
			val = payload[addr] + (payload[addr+1] << 8);
			printf("0x%04x -> 0x%04x\n", val, val + DESTINATION_SEGMENT);
			val += DESTINATION_SEGMENT;
			payload[addr] = val & 0xff;
			payload[addr+1] = val >> 8;
		}

		// bootsector.asm always points ES to 0x100 bytes before load image.
		// But DS will be equal to CS for .COM executables, but for .EXEs, it
		// must equals ES.
		initial_ds = DESTINATION_SEGMENT - 0x10;

	} else {
		printf("Reading com file...\n");
		// .com origin is at 0x100
		// If there are command line options, we could place them
		// somewhere here like DOS does....
		memset(payload, 0, 0x100);

		payload[0x80] = 1;	// count of characters in command tail
		payload[0x81] = 0x0D;	// all characters entered after the program name followed by a CR byte

		payload_size =  fread(payload + 0x100, 1, 0x10001-0x100, fptr_payload);
		payload_size += 0x100;
		if (payload_size < 0) {
			perror("fread");
			goto err;
		}
		if (payload_size > 0x10000) {
			fprintf(stderr, "Payload too large");
			goto err;
		}
	}

	printf("Payload size: %d\n", payload_size);
	printf("Code : 0x%04x:0x%04x\n", initial_cs, initial_ip);
	printf("Stack : 0x%04x:0x%04x\n", initial_ss, initial_sp);


	for (i=0; i<payload_size-1; i++) {
			if (payload[i] == 0xCD) {
			unsigned char intno = payload[i+1];

			if ((intno >= 0x20 && intno <= 0x29) || intno == 0x2E || intno == 0x2F) {
				printf("Warning: Potential DOS %02xh interrupt call at 0x%04x\n", intno, i);
				hexdump(&payload[i>16 ? (i-16) :0], 24, i>16 ? (i-16):0);
				dos_interrupts = 1;

				if (breakpoints_ints_fptr) {
					fprintf(breakpoints_ints_fptr, "vbreak 0x%04x:0x%04x\n", DESTINATION_SEGMENT, i);
				}
			}
		}
	}

	/** Write the file */
	fptr_disk = fopen(output_file, "wb");
	if (!fptr_disk) {
		perror("fopen");
		goto err;
	}

	printf("Writing %s...\n", output_file);
	// 1 : Number of sectors to copy
	if (payload_size < 512) {
		n_sectors = 1;
	} else {
		n_sectors = payload_size / 512 + 1;
	}
	printf("Bootstrap: %d sectors to copy, %d sectors per track\n\tinitial IP 0x%04x\n", n_sectors, sectors_per_track, initial_ip);
	printf("\tdestination segment 0x%04x\n", DESTINATION_SEGMENT);
	printf("\tinitial SP 0x%04x\n", initial_sp);
	printf("\tinitial CS 0x%04x\n", initial_cs);
	printf("\tinitial DS 0x%04x\n", initial_ds);
	printf("\tinitial SS 0x%04x\n", initial_ss);

	bootstrap_write(n_sectors, DESTINATION_SEGMENT,
			initial_ip, initial_sp, initial_cs, initial_ds, initial_ss,
			&bpb,
			disk_label,
			fptr_disk);

	if (1)
	{
		int root_dir_sectors = bpb.root_dir_entries * 32 / bpb.bytes_per_logical_sector;
		int track_size = bpb.sectors_per_track * bpb.num_heads;

		first_cluster_sector = bpb.reserved_logical_sectors + bpb.logical_sectors_per_fat * bpb.n_fats + root_dir_sectors;

		printf("First sector: 0x%04x\n", first_cluster_sector);
		printf("Sectors per track: 0x%04x\n", track_size);
		if (first_cluster_sector % track_size) {
			first_cluster_sector = (first_cluster_sector / track_size + 1) * track_size;
			printf("Target sector: 0x%04x\n", first_cluster_sector);
		}
		printf("Payload starting at track 0x%04x\n", first_cluster_sector / track_size);
		fseek(fptr_disk, first_cluster_sector * bpb.bytes_per_logical_sector, SEEK_SET);
//		fseek(fptr_disk, 1*512, SEEK_SET);
	}

	fseek(fptr_disk, first_cluster_sector * bpb.bytes_per_logical_sector, SEEK_SET);
	fwrite(payload, payload_size, 1, fptr_disk);


	if (bpb.disk_image_size > 512+payload_size) {
		fseek(fptr_disk, bpb.disk_image_size-1, SEEK_SET);
		// Pad file size
		payload[0] = 0;
		fwrite(payload, 1, 1, fptr_disk);
	}

	if (1)
	{
		int fat_size_bytes = bpb.bytes_per_logical_sector * bpb.logical_sectors_per_fat;
		uint8_t fat[fat_size_bytes];
		int cluster;
		int n_reserved_clusters = (first_cluster_sector * bpb.bytes_per_logical_sector + payload_size) / bpb.bytes_per_logical_sector / bpb.sectors_per_cluster;
		int directory_offset;

		directory_offset = (bpb.reserved_logical_sectors + bpb.logical_sectors_per_fat * bpb.n_fats) * bpb.bytes_per_logical_sector;

		printf("Payload size: 0x%04x\n", payload_size);
		printf("Payload first sector: 0x%04x\n", first_cluster_sector);
		printf("Reserving %d clusters (0x%04x bytes)\n",
				n_reserved_clusters,
				n_reserved_clusters * bpb.sectors_per_cluster * bpb.bytes_per_logical_sector);

		memset(fat, 0, sizeof(fat));
		fat[0] = bpb.media_descriptor;
		fat[1] = 0xFF;
		fat[2] = 0xFF;
		for (cluster=2; cluster<n_reserved_clusters ; cluster++) {
			int fat_entry = 0xFF7;
			if (cluster&1) {
				fat[cluster/2*3+2] = fat_entry >> 4;
				fat[cluster/2*3+1] |= fat_entry << 4;
			} else {
				fat[cluster/2*3] = fat_entry;
				fat[cluster/2*3+1] = fat_entry>>8;
			}
		}

		dos_interrupts = 0;
		fseek(fptr_disk,
				bpb.bytes_per_logical_sector * bpb.reserved_logical_sectors, SEEK_SET);
		fwrite(fat, sizeof(fat), 1, fptr_disk);
		if (bpb.n_fats > 1)
			fwrite(fat, sizeof(fat), 1, fptr_disk);

		printf("Directory table offset: 0x%04x\n", directory_offset);
		fseek(fptr_disk, directory_offset, SEEK_SET);
		writeVolumeLabel(fptr_disk, disk_label, 0x08, 0, 0, 0, 0);
	}


	fclose(fptr_disk);
	retval = 0;
err:
	fclose(fptr_payload);

	if (dos_interrupts) {
		printf("* * * * * * *\n");
		printf("Warning: Byte sequences looking like DOS interrupt calls (eg: CD 21   int 21h ) were found!\n");
		printf("\n");
		printf("This may be a coincidence as code and data are undistinguishable without more analysis. If "
				"your software does not boot, you should disassemble your executable and investigate.\n"
				"See above for int number(s) and address(es).\n");
		printf("\n");
		printf("Note that int 21h (ah=09h, ah=25h and ah=35h) are supported by bootsector.asm.\n");
		printf("* * * * * * *\n");
	}

	if (breakpoints_ints_fptr) {
		fclose(breakpoints_ints_fptr);
	}

	return retval;
}
