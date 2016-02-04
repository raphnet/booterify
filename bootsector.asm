org 7c00h
bits 16
cpu 8086
[map symbols bootsector.map]

%define DESTINATION_SEGMENT	1000H
%define FIRST_SECTOR		2
%define FIRST_TRACK			0

init:
	jmp 0000:start

sectors_to_copy: dw 128
sectors_per_track: db 9
initial_ip: dw 0100h
initial_sp: dw 0FFFEh
initial_cs: dw DESTINATION_SEGMENT
initial_ds: dw DESTINATION_SEGMENT
initial_ss: dw DESTINATION_SEGMENT

start:
	; This boot code is at 0000:7c00 (512 bytes)
	;
	cli

	; Setup stack pointer at the end of our 512 byte space
	mov ax, 0
	mov ds, ax
;	mov ss, ax
;	mov sp, 07CFEh

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

	; Setup registers for the loop
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
	jg _complete_track ; al stays untouched
	mov al, [sectors_to_copy]
_complete_track:
	mov ah, 02h
	int 13h
	jc error

	push ax
	push cx
		; AL equals the number of sectors that were just read
		xor ah,ah
		sub word [sectors_to_copy], ax

		; Multiply by 512 and increment BX
		mov cl, 9
		shl ax, cl
		add bx, ax
	pop cx
	pop ax

	; Prepare values for the next phase.
	mov cl, 1 ; all reads now start at track 1
	mov al, [sectors_per_track] ; read complete tracks

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

	; Data segment
	mov dx, [initial_ds]
	mov ds, dx
	mov es, ax ; Needed?

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

section .marker start=0x7dFE
	db 0x55, 0xaa

section .data
