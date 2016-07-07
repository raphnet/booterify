NASM=nasm
CC=gcc
LD=$(CC)

all: bootsector.bin booterify exeinfo

bootsector.bin: bootsector.asm
	nasm $< -fbin -o $@ -O0
	ls -lh $@

booterify: booterify.o bootstrap.o
	$(LD) $(LDFLAGS) $^ -o $@

exeinfo: exeinfo.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f bootsector.bin *.o booterify exeinfo
