NASM=nasm
CC=gcc
LD=$(CC)
include version.inc
CFLAGS=-Wall -O0 -DVERSION=\"$(VERSION)\"

all: bootsector.bin booterify exeinfo

bootsector.bin: bootsector.asm
	nasm $< -fbin -o $@ -O0 -DVERSION=\"$(VERSION)\"
	ls -lh $@

booterify: booterify.o bootstrap.o
	$(LD) $(LDFLAGS) $^ -o $@

exeinfo: exeinfo.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f bootsector.bin *.o booterify exeinfo
