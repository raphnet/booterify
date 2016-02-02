NASM=nasm

all: bootsector.bin booterify

bootsector.bin: bootsector.asm
	nasm $< -fbin -o $@ -O0
	ls -lh $@

booterify: booterify.c
	gcc booterify.c -o booterify

clean:
	rm -f bootsector.bin
