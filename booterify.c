#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include "bootstrap.h"

#define DESTINATION_SEGMENT	0x1000

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
	printf(" -c             Force input file format as .com (default: auto-detect)\n");
	printf(" -e             Force input file format as .exe (default: auto-detect)\n");
	printf(" -s             Sectors per track\n");
	printf(" -t             Target disk size (padded with zeros). If 0, no padding.\n");
	printf("\n");
	printf("Debugging/Development:\n");
	printf(" -B file        Generate list of breakpoints commands for bochs for all\n");
	printf("                instances of DOS int calls\n");
	printf("\n");
	printf("Common disk parameters:\n");
	printf("  360k floppy:  -s 9 -t 368640\n");
	printf("  1.44MB floppy:  -s 18 -t 1474560\n");
	printf("\n");
	printf("Note: Padding with -t as above is not required when writing to\n");
	printf("      a physical floppy, but it can help for emulators or virtual\n");
	printf("      machines that use the file size to determine the floppy type.\n");
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
	unsigned int disk_image_size = 0;
	int opt, i, retval = -1;
	char *e;
	const char *bootstrap_file, *executable_file, *output_file;
	uint16_t exe_header[14];
	char is_exe;
	int dos_interrupts = 0;
	FILE *breakpoints_ints_fptr = NULL;

	while (-1 != (opt = getopt(argc, argv, "hces:t:i:B:"))) {
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
			case 's':
				sectors_per_track = strtol(optarg, &e, 0);
				if (e==optarg) {
					fprintf(stderr, "Invalid value specified\n");
					return 1;
				}
				break;

			case 't':
				disk_image_size = strtol(optarg, &e, 0);
				if (e==optarg) {
					fprintf(stderr, "Invalid value specified\n");
					return 1;
				}
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
	printf("Output file padding: %d B / %.2f kB / %.2f MB)\n", disk_image_size, disk_image_size / 1024.0, disk_image_size / 1024.0 / 1024.0);

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
	bootstrap_write(n_sectors, sectors_per_track, DESTINATION_SEGMENT,
			initial_ip, initial_sp, initial_cs, initial_ds, initial_ss,
			fptr_disk);
	fwrite(payload, payload_size, 1, fptr_disk);
	if (disk_image_size > 512+payload_size) {
		fseek(fptr_disk, disk_image_size-1, SEEK_SET);
		// Pad file size
		payload[0] = 0;
		fwrite(payload, 1, 1, fptr_disk);
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
