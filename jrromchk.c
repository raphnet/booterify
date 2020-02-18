#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define JRC_SIG	"PCjr Cartridge image file\r\n"
#define JRC_CREATOR	"Booterify " VERSION

struct jrc_header {
	uint8_t signature[27+1];
	char creator[30+1];
	char comment[400+1];
	uint8_t version[2];
	uint16_t address;
	uint16_t adrmask;
	uint8_t reserved[46];
};

struct pcjr_rom {
	uint8_t *data;
	int offset;
	int romsize;
	int crc_ok;
	uint16_t crc;
	int has_jrc_header;
	struct jrc_header jrc;
};

void loadJrcHeader(uint8_t *data, struct jrc_header *dst)
{
	memset(dst, 0, sizeof(struct jrc_header));
	memcpy(dst->signature, data, sizeof(dst->signature)-1);
	memcpy(dst->creator, data+27, sizeof(dst->creator)-1);
	memcpy(dst->comment, data+59, sizeof(dst->comment)-1);
	dst->version[0] = data[460];
	dst->version[1] = data[461];
	dst->address = data[462] | (data[463] << 8);
	dst->adrmask = data[464] | (data[465] << 8);
}

void writeJRCheader(FILE *fptr, const struct jrc_header *jrc)
{
	uint8_t tmp[401];

	// Signature
	fprintf(fptr, JRC_SIG);

	// Creator + CR
	fprintf(fptr, "%-30s\r\n", JRC_CREATOR);

	// Comment (terminated by 0x1A)
	memset(tmp, 0x1A, sizeof(tmp));
	memcpy(tmp, jrc->comment, strlen(jrc->comment));
	fwrite(tmp, 400, 1, fptr);

	// EOF
	fprintf(fptr, "%c", 0x1A);

	// Version
	fwrite(jrc->version, 2, 1, fptr);

	// Address
	tmp[0] = jrc->address;
	tmp[1] = jrc->address >> 8;
	fwrite(tmp, 2, 1, fptr);

	// Address mask
	tmp[0] = jrc->adrmask;
	tmp[1] = jrc->adrmask >> 8;
	fwrite(tmp, 2, 1, fptr);

	// Reserved
	memset(tmp, 0x00, 46);
	fwrite(tmp, 46, 1, fptr);

}

void buildJRCHeader(struct jrc_header *jrc, uint16_t address, uint16_t mask)
{
	memset(jrc, 0, sizeof(struct jrc_header));
	jrc->version[0] = 1;
	jrc->version[1] = 0;
	jrc->address = address;
	jrc->adrmask = mask;
}

void printJrcHeader(const struct jrc_header *jrc)
{
	printf("Creator.........: %s\n", jrc->creator);
	printf("Comment.........: %s\n", jrc->comment);
	printf("Image version...: %d.%d\n", jrc->version[0], jrc->version[1]);
	printf("Segment address.: 0x%04x\n", jrc->address);
	printf("Address mask....: 0x%04x\n", jrc->adrmask);
}

uint16_t crc_xmodem_update(uint16_t crc, uint8_t data)
{
	int i;

	crc = crc ^ ((uint16_t)data << 8);
	for (i=0; i<8; i++)
	{
		if (crc & 0x8000)
			crc = (crc << 1) ^ 0x1021;
		else
			crc <<= 1;
	}

	return crc;
}

uint16_t crc_compute(const unsigned char *data, int len)
{
	uint16_t crc = 0xFFFF;
	int i;

	for (i=0; i<len; i++) {
		crc = crc_xmodem_update(crc, *data);
		data++;
	}

	return crc;
}

int crc_check(unsigned char *data, int checklen, int verbose)
{
	uint16_t crc = 0xFFFF;

	crc = crc_compute(data, checklen);

	if (crc != 0) {
		fprintf(stderr, "CRC ERROR 0x%04x\n", crc);
		return -1;
	}

	if (verbose) {
		printf("CRC OK\n");
	}

	return 0;
}

int writeROM(const char *out_filename, struct pcjr_rom *rom, int jrc_format)
{
	FILE *fptr;

	fptr = fopen(out_filename, "wb");
	if (!fptr) {
		perror(out_filename);
		return -1;
	}

	if (jrc_format) {

		if (!rom->has_jrc_header) {
			buildJRCHeader(&rom->jrc, 0xE000, 0x0000);
		}

		writeJRCheader(fptr, &rom->jrc);
	}


	if (fwrite(rom->data + rom->offset, rom->romsize, 1, fptr) != 1) {
		perror("Error writing ROM data\n");
		fclose(fptr);
		return -1;
	}

	fclose(fptr);

	return 0;
}

/**
 * \param from_data The source data (eg: read from a file) to load the ROM from.
 * \param size The size of the source data
 * \param dst The destination structure
 * \param verbose Verbose output if non-zero
 */
int loadRom(unsigned char *from_data, int size,struct pcjr_rom *dst, int verbose, int fix_size)
{
	uint8_t magic[2] = { 0x55, 0xAA };

	memset(dst, 0, sizeof(struct pcjr_rom));

	dst->data = from_data;

	// Check for presence of a JRC file header
	if (size >= 512) {
		if (0 == memcmp(JRC_SIG, from_data, strlen(JRC_SIG))) {
			loadJrcHeader(from_data, &dst->jrc);

			if (verbose) {
				printf("Detected a JRC header\n");
				printJrcHeader(&dst->jrc);
			}

			dst->offset = 512; // skip the header
		}
	}

	//
	// [0-1] : 55AA
	// [2]   : Length (length / 512)
	// code!
	//

	if (size < (dst->offset + 3)) {
		fprintf(stderr, "ROM too small to be valid\n");
		return -1;
	}

	if (memcmp(&from_data[dst->offset], magic, sizeof(magic))) {
		fprintf(stderr, "ROM does not start with 55AA\n");
		return -1;
	}

	if (fix_size) {
		dst->romsize = (size-dst->offset);
		from_data[dst->offset + 2] = dst->romsize / 512;
		if (verbose) {
			printf("Patched size byte to %d\n", from_data[dst->offset + 2]);
		}
	}
	else {
		dst->romsize = from_data[dst->offset + 2] * 512;
		if (dst->romsize < 1) {
			fprintf(stderr, "Error: A rom size of 0 does not make sense\n");
			return -1;
		}
	}

	if (verbose) {
		printf("File size: %d\n", size);
		printf("Offset: %d\n", dst->offset);
		printf("ROM size: %d\n", dst->romsize);
	}

	// The file could be larger if, for instance, a 16K game was
	// burned to a 32K eeprom. But the file may not be smaller.
	if (dst->romsize > (size - dst->offset)) {
		fprintf(stderr, "Error: ROM declares a size larger than the file\n");
		return -1;
	}

	dst->crc = crc_compute(from_data + dst->offset, dst->romsize);

	if (dst->crc == 0) {
		dst->crc_ok = 1;
	}

	return 0;
}

void setRomCRC(struct pcjr_rom *rom, uint16_t crc)
{
	rom->data[rom->offset + rom->romsize - 2] = crc >> 8;
	rom->data[rom->offset + rom->romsize - 1] = crc;
}

void printHelp(void)
{
	printf("jrromchk, part of Booterify version %s\n\n", VERSION);
	printf("Usage: ./jrromchk [options] inputFile\n");
	printf("\n");
	printf(" -h        Print usage information and exit\n");
	printf(" -v        Enable verbose output\n");
	printf(" -p        Compute CRC and patch the file\n");
	printf(" -s        Fix the size byte based on input file size\n");
	printf(" -i        Display information about the file (default)\n");
	printf(" -o file   Write (modified) ROM to file (default: Binary format)\n");
	printf(" -j        Enable JRC format output (use with -o)\n");
}

int main(int argc, char **argv)
{
	FILE *fptr;
	unsigned char *data;
	long size;
	int alloc_size;
	int res;
	int opt;
	int patch_crc = 0;
	int verbose = 0;
	int fix_size = 0;
	int jrc_output = 0;
	struct pcjr_rom rom;
	const char *outfilename = NULL;


	while ((opt = getopt(argc, argv, "vipho:sj")) != -1) {
		switch(opt)
		{
			case 'j':
				jrc_output = 1;
				break;
			case 'o':
				outfilename = optarg;
				break;
			case 's':
				fix_size = 1;
				break;
			case 'v':
				verbose =1;
				break;
			case 'h':
				printHelp();
				return 0;
			case 'i':
				break;
			case 'p':
				patch_crc = 1;
				break;
			default:
				fprintf(stderr, "Unknown option. Try -h\n");
				return 1;
		}
	}

	if (optind >= argc) {
		printf("No file specified\n");
		return 1;
	}

	fptr = fopen(argv[optind], "rb");
	if (!fptr) {
		perror(argv[1]);
		return -1;
	}

	fseek(fptr, 0, SEEK_END);
	size = ftell(fptr);
	alloc_size = size;
	fseek(fptr, 0, SEEK_SET);

	// Round up to 512 bytes
	if (fix_size) {
		alloc_size = (size + 511) / 512 * 512;

		if (verbose) {
			printf("Input file size is %d\n", (int)size);
			printf("Rounded up to %d\n", (int)alloc_size);
		}
	}

	data = malloc(alloc_size);
	if (!data) {
		perror("could not allocate memory for file");
		fclose(fptr);
		return -1;
	}

	memset(data, 0xff, alloc_size);

	if (1 != fread(data, size, 1, fptr)) {
		perror("could not read file");
		free(data);
		fclose(fptr);
		return -1;
	}

	res = loadRom(data, alloc_size, &rom, verbose, fix_size);
	if (res) {
		printf("ROM INVALID\n");
		fclose(fptr);
		return -1;
	}

	if (rom.crc_ok) {
		printf("ROM OK\n");
		res = 0;
	} else {

		// CRC invalid. But do we correct it?
		if (patch_crc) {
			uint16_t crc = crc_compute(rom.data + rom.offset, rom.romsize-2);

			setRomCRC(&rom, crc);

			res = loadRom(data, alloc_size, &rom, verbose, 0);
			if (res || !rom.crc_ok) {
				printf("ROM INVALID - Failed to fix it? (bug?)\n");
				res = -1;
			} else {
				printf("ROM OK (CRC PATCHED)\n");
				res = 0;
			}
		} else {
			printf("ROM INVALID (BAD CRC)\n");
			res = -1;
		}
	}

	// at this point, if res is 0, we have a perfectly valid ROM to
	// write to the output file, if specified.
	if ((res == 0) && outfilename) {
		res = writeROM(outfilename, &rom, jrc_output);
		if (res == 0) {
			printf("Wrote %s\n", outfilename);
		} else {
			fprintf(stderr, "Could not write ROM\n");
		}
	}

	fclose(fptr);

	return res;
}
