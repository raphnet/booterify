org 0h
bits 16
cpu 8086
[map symbols map.map]

%define DESTINATION_SEGMENT	07E0H
%define FIRST_SECTOR		2
%define FIRST_TRACK			0
%define SECTOR_SIZE			512
%define NUM_SECTORS_TO_COPY	payload_size / SECTOR_SIZE

start:

	; Setup
	;
	; This boot code is at 0000:7c00 (512 bytes)
	;
	; The payload is to be installed in a nice 64K segment
	; right after this boot code.
	;
	; 0000:7e00 or rather 07e0:0000
	;
	cli

	; Setup stack pointer at the end of our 512 byte space
	mov ax, 0
	mov ds, ax
	mov ss, ax
	mov sp, 01ffh

	; Trace
	mov ah, 00Eh
	mov al, '1'
	mov bh, 0
	mov bl, 1
	mov cx, 1
	int 10h

	; ES:BX is the destination
	mov ax, DESTINATION_SEGMENT
	mov es, ax
	mov bx, 0h

	mov word [sector_count], 0
	mov cl, FIRST_SECTOR
	mov ch, FIRST_TRACK
	mov dh, 0 ; Side
copy_loop:

	push ax
	push bx
	push cx
		mov ah, 00Eh
		mov bh, 0
		mov bl, 1
		mov cx, 1

		mov cx, [sector_count]
		and cx, 0xf
		jnz nn
		mov al, 13
		int 10h
		mov al, 10
		int 10h
nn:
		mov al, '.'
		int 10h

	pop cx
	pop bx
	pop ax

	push bx
	push cx
	push dx
		mov al, 1 ; Always read one sector
		mov ah, 02h ; INT 13H 02H : Read sectors
		mov dl, 0 ; Todo: Check if we can know which floppy we've been loaded from
		int 13h ; Read the sectors
	pop dx
	pop cx
	pop bx
	jc error

	add bx, SECTOR_SIZE

	inc word [sector_count]
	cmp word [sector_count], NUM_SECTORS_TO_COPY
	je loading_done

	inc cl
	cmp cl, 9
	jg _next_side
	jmp copy_loop

_next_side:
	mov cl, 1 ; start at sector 1 on next head
	inc dh
	cmp dh, 2
	jge _next_track
	jmp copy_loop

_next_track:
	inc ch
	mov cl, 1 ; back to sector 1
	mov dh, 0 ; back to head 0
	jmp copy_loop

loading_done:
	mov ah, 00Eh
	mov al, '2'
	mov bh, 0
	mov bl, 1
	mov cx, 1
	int 10h

start_payload:
	; Setup stack at end of destination 64K block
	mov ax, DESTINATION_SEGMENT
	mov ss, ax
	mov ds, ax
	mov es, ax
	mov sp, 0FFFEh

	; Go!
	jmp DESTINATION_SEGMENT:0100h

error:
	mov al, ah ; Return code from int13h
	add al, 'A'
	mov ah, 00Eh
	mov bh, 0
	mov bl, 1
	mov cx, 1
	int 10h

hang:
	jmp hang

sector_count: resw 1

section .marker start=0x1FE
	db 0x55, 0xaa

section .data

payload:
	times 100h db 0x00
	;incbin "../dosgames/ratillry.com"
	;incbin "/mnt/yggdrasil/games/pc/mspacman/MSPACMAN.COM"
	incbin "/mnt/yggdrasil/games/pc/pipes/pipes.com"
payload_size: equ $-payload

section .pading start=368639
	db 0xff
