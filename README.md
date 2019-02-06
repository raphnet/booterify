# Booterify: Bootloader for 16-bit .COM and .EXE executables

Booterify is a simple bootloader that fits in the first sector of a floppy (512 bytes) and a tool to create boot disks form a DOS executable (.COM or .EXE).

## Features

- Supports .COM executables
- Supports .EXE executables (64 kB max.)
- Provides a BPB (Bios parameter block) to enable booting from USB keys
- Detects potential DOS interrupt calls, displays their address and hex dump around the call
- Can generate a list of breakpoints in bochs syntax for each DOS service call instance
- Implemented DOS services:
  - int 21h/AH=02h : Display character
  - int 21h/AH=09h : Display string
  - int 21h/AH=25h : Get interrupt vector
  - int 21h/AH=35h : Set interrupt vector

## For more information...

Visit the [project homepage](http://www.raphnet.net/programmation/booterify/index_en.php) for example
uses, etc.

## License

MIT License.
