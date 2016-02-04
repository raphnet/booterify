#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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

#define PRINT_HEADER_ITEM(a)	printf(#a": 0x%04x\n", header[a])


int read_uint16le(FILE *fptr, uint16_t *dst)
{
	unsigned char buf[2];

	if (1 != fread(buf, 2, 1, fptr)) {
		perror("fread");
		return -1;
	}

	*dst = buf[0] | (buf[1]<<8);

	return 0;
}

int main(int argc, char **argv)
{
	FILE *fptr, *out_fptr = NULL;
	int retval = 0;
	int i;
	uint16_t header[14];
	int file_size;
	int load_module_size;
	const char *loadmodule_outfn = NULL;

	if (argc<2) {
		printf("Usage: ./exeinfo file.exe [loadmodule.bin]\n");
		printf("\n");
		printf("The 3rd argument is optional. Used to extract the load module from the .exe\n");
		return 1;
	}

	if (argc >= 3) {
		loadmodule_outfn = argv[2];
		printf("Loadmodule output file: %s\n", loadmodule_outfn);
	}

	fptr = fopen(argv[1], "rb");
	if (!fptr) {
		perror("fopen");
		return 2;
	}

	for (i=0; i<14; i++) {
		if (read_uint16le(fptr, &header[i])) {
			retval = -1;
			goto err;
		}
	}

	if (header[SIG] != 0x5a4d) {
		fprintf(stderr, "Not an MZ executable\n");
		retval = -1;
		goto err;
	}

	PRINT_HEADER_ITEM(SIG);
	PRINT_HEADER_ITEM(LAST_PAGE_SIZE);
	PRINT_HEADER_ITEM(FILE_PAGES);
	PRINT_HEADER_ITEM(RELOCATION_ITEMS);
	PRINT_HEADER_ITEM(HEADER_PARAGRAPHS);
	PRINT_HEADER_ITEM(MINALLOC);
	PRINT_HEADER_ITEM(MAXALLOC);
	PRINT_HEADER_ITEM(INITIAL_SS);
	PRINT_HEADER_ITEM(INITIAL_SP);
	PRINT_HEADER_ITEM(CHECKSUM);
	PRINT_HEADER_ITEM(INITIAL_IP);
	PRINT_HEADER_ITEM(PRE_RELOCATE_CS);
	PRINT_HEADER_ITEM(RELOCATION_TABLE_OFFSET);
	PRINT_HEADER_ITEM(OVERLAY_NUMBER);

	file_size = header[FILE_PAGES]*512;
	if (header[LAST_PAGE_SIZE]) {
		file_size -= 512 - header[LAST_PAGE_SIZE];
	}
	printf("File size: %d\n", file_size);

	load_module_size = file_size - header[HEADER_PARAGRAPHS] * 16;

	printf("Load module size: %d\n", load_module_size);

	fseek(fptr, header[RELOCATION_TABLE_OFFSET], SEEK_SET);
	for (i=0; i<header[RELOCATION_ITEMS]; i++) {
		uint16_t offset, segment;

		read_uint16le(fptr, &offset);
		read_uint16le(fptr, &segment);
		printf("Relocation %d : %04x %04x\n", i, offset, segment);
	}

	if (loadmodule_outfn && load_module_size>0) {
		unsigned char *lm;
		int chunk_size, todo = load_module_size;

		out_fptr = fopen(loadmodule_outfn, "wb");
		if (!out_fptr) {
			perror("fopen");
			retval = -1;
			goto err;
		}

		lm = malloc(2048);
		if (!lm) {
			perror("malloc");
			retval = -1;
			goto err;
		}
		fseek(fptr, header[HEADER_PARAGRAPHS] * 16, SEEK_SET);

		do {
			chunk_size = todo > 2048 ? 2048 : todo;
			fread(lm, chunk_size, 1, fptr);
			fwrite(lm, chunk_size, 1, out_fptr);
			printf("Chunk %d\n", chunk_size);
			todo -= chunk_size;
		} while (todo);

		free(lm);
	}

err:
	if (out_fptr) {
		free(out_fptr);
	}
	fclose(fptr);

	return retval;
}
