org 7c00h
bits 16
cpu 8086
[map symbols bootsector.map]

%define INT21H_HELPERS

%define DESTINATION_SEGMENT	1000H
%define FIRST_SECTOR		2
%define FIRST_TRACK			0

section .text

init:
	jmp short start
	nop
	db 'raphnet '	; OEM identifier
	dw 512	; Bytes per logical sector
	db 1	; Logical sectors per cluster
	dw 129	; Reserved logical sectors
	db 2	; Number of FATs
	dw 224	; Root directory entries
	dw 2880	; Total logical sectors
media_descriptor:	db 0xF0 ; Media descriptor
	dw 0	; Logical sectors per fat
sectors_per_track: db 9
	db 0
num_heads:
	dw 2	; Number of heads
	dd 0 	; Hiden sectors
	dd 0	; Large number of sectors

	db 0	; Drive number
	db 0	; Winnt flags
signature:	db 0x28	; Signature
	db '0000' ; Volume ID/Serial
	db '           ' ; Label
	db 'FAT12   '	; System identifier string

nop
nop

sectors_to_copy: dw 128
initial_ip: dw 0100h
initial_sp: dw 0FFFEh
initial_cs: dw DESTINATION_SEGMENT
initial_ds: dw DESTINATION_SEGMENT
initial_ss: dw DESTINATION_SEGMENT

start:
	jmp 0000:start2
start2:
	; This boot code is at 0000:7c00 (512 bytes)
	;
	cli

	; Setup data segment to 0000
	mov ax, 0
	mov ds, ax

	; Setup stack
	mov ss, ax
	mov sp, 7C00h

%ifdef INT21H_HELPERS
	; Install our handler at 0000:0084
	mov ax, int21
	mov word [21h * 4], ax
	mov ax, 0
	mov word [21h * 4 + 2], ax
%endif

	; Trace
	mov ah, 00Eh
	mov al, '1'
	mov bh, 0
	mov bl, 1
	mov cx, 1
	int 10h

	; ES:BX is the destination
	mov ax, [initial_cs] ; DESTINATION_SEGMENT
	mov es, ax
	mov bx, 0h

	; Setup registers for the loop
	; DL is never touched. Should still hold boot drive number
	mov ch, FIRST_TRACK
	mov dh, 0 ; Side
	mov cl, FIRST_SECTOR ; the first read start after the loader
	mov al, [sectors_per_track]
	sub al, 1 ; Loader sector skipped
	jmp _same_track

copy_loop:
	call progress
	xor dh, 1 ; Toggle head
	jne _same_track
	inc ch ; Increment track when returning to head 0
_same_track:
	push ax
	xor ah,ah
	mov al, [sectors_per_track]
	cmp word [sectors_to_copy], ax
	pop ax
	jge _complete_track ; al stays untouched
	mov al, [sectors_to_copy]
_complete_track:
	mov ah, 02h
	push ax ; Keep AL around. Not all BIOSes preserve it.
		int 13h
	pop ax
	jc error

	push cx
		; AL equals the number of sectors that were just read
		xor ah,ah
		sub word [sectors_to_copy], ax

		; Multiply by 512 and increment BX
		mov cl, 9
		shl ax, cl
		add bx, ax
	pop cx

	; Prepare values for the next phase.
	mov al, [sectors_per_track]
	mov cl, 1 ; all reads now start at track 1

	test word [sectors_to_copy], 0xffff
	jnz copy_loop


loading_done:
	mov ah, 00Eh
	mov al, '2'
	mov bh, 0
	mov bl, 1
	mov cx, 1
	int 10h

start_payload:

	; Setup stack
	mov ax, [initial_ss]
	mov ss, ax
	mov sp, [initial_sp]

	; Put far return address on stack
	mov dx, [initial_cs]
	push dx
	mov dx, [initial_ip]
	push dx

	mov ax, [initial_cs]
	sub ax, 0x10
	mov es, ax
	xor ax,ax

	; Data segment
	mov dx, [initial_ds]
	mov ds, dx


final_jump:
	retf

progress:
	push ax
	push bx
	mov ah, 0Eh
	mov al, '.'
	mov bh, 0
	mov bl, 1
	int 10h
	pop bx
	pop ax
	ret

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

%ifdef INT21H_HELPERS
int21:
	cmp ah, 25h
	je int21_25
	cmp ah, 35h
	je int21_35
	cmp ah, 09h
	je int21_09
	iret

	; AH : 09
	; DX : $tring
int21_09:
	push ax
	push bx
		mov bx, dx
		mov ah, 0Eh
_int21_09_lp:
		mov al, [bx]
		push bx
			mov bx, 0x0001
			int 10h
		pop bx

		inc bx
		cmp byte [bx], '$'
		jne _int21_09_lp
	pop bx
	pop ax
	iret

	; AH: 25h
	; AL: interrupt number
	; DS:DX -> new interrupt handler
int21_25:
	push ax
	push es
	push di

	xor ah,ah
	shl ax, 1
	shl ax, 1
	mov di, ax

	mov ax, 0
	mov es, ax

	cld
	mov ax, dx
	stosw ; offset
	mov ax, ds
	stosw ; segment

	pop di
	pop es
	pop ax
	iret

	; AH : 35h
	; AL : Interrupt number
	;
	; Return
	; ES:BX -> current interrupt handler
int21_35:
	push ax
	push ds
	push si

	xor ah,ah
	shl ax, 1
	shl ax, 1
	mov si, ax

	mov ax, 0
	mov ds, ax

	cld
	lodsw ; offset
	mov bx, ax
	lodsw ; segment
	mov es, ax

	pop si
	pop ds
	pop ax
	iret
%endif

section .marker start=0x7dFE
	db 0x55, 0xaa

section .data
