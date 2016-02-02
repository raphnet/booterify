#include <stdio.h>

unsigned char payload[0x10000];

int main(int argc, char **argv)
{
	FILE *fptr_bootstrap, *fptr_payload;
	unsigned char bootstrap_buf[513];
	int bootstrap_size;
	int payload_size;

	if (argc < 3) {
		printf("Usage: ./booterify bootstrap.bin file.com\n");
		return 0;
	}

	fptr_bootstrap = fopen(argv[1], "rb");
	if (!fptr_bootstrap) {
		perror("could not open bootstrap");
		return -1;
	}

	fptr_payload = fopen(argv[1], "rb");
	if (!fptr_payload) {
		perror("could not open payload");
		fclose(fptr_bootstrap);
		return -1;
	}

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

	bootstrap_size =  fread(bootstrap_buf, 1, 0x10001, fptr_bootstrap);
	if (bootstrap_size < 0) {
		perror("fread");
		goto err;
	}
	if (bootstrap_size > 0x10000) {
		fprintf(stderr, "Payload too large");
		goto err;
	}

	printf("Payload size: %d\n", bootstrap_size);


err:
	fclose(fptr_bootstrap);
	fclose(fptr_payload);

	return 0;
}
