NASM=nasm
CC=gcc
LD=$(CC)
include version.inc
CFLAGS=-Wall -O0 -DVERSION=\"$(VERSION)\"

all: bootsector.bin booterify exeinfo jrromchk pcjrloader.bin bin2exe

bootsector.bin: bootsector.asm
	nasm $< -fbin -o $@ -O0 -DVERSION=\"$(VERSION)\" -l bootsector.lst
	ls -lh $@

pcjrloader.bin: pcjrloader.asm
	nasm $< -fbin -o $@ -O0 -DVERSION=\"$(VERSION)\" -l pcjrloader.lst
	ls -lh $@

booterify: booterify.o bootstrap.o
	$(LD) $(LDFLAGS) $^ -o $@

exeinfo: exeinfo.o
	$(LD) $(LDFLAGS) $^ -o $@

jrromchk: jrromchk.o
	$(LD) $(LDFLAGS) $^ -o $@

bin2exe: bin2exe.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f bootsector.bin *.o booterify exeinfo pcjrloader.bin jrromchk
