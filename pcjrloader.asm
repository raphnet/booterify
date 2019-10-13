org 0x0000
bits 16
cpu 8086
[map symbols pcjrloader.map]

%define INT21H_HELPERS

; Start after the BIOS Data Area
; use one paragraph for the stack (SS=0050)

%define DESTINATION_SEGMENT	0051H

; Payload start must be at the first
; sector of track 1
%define FIRST_SECTOR		1
%define FIRST_TRACK			1

section .text

	db 0x55, 0xAA	; Marker
	db 0			; Size / 512. Will be patched by jrromchk
	jmp start		; Code follows

	times 58 db 0 ; Todo : Align to match bootsector.asm (40h)

sectors_to_copy: dw 128
destination_segment: dw DESTINATION_SEGMENT
initial_ip: dw 0100h
initial_sp: dw 0FFFEh
initial_cs: dw DESTINATION_SEGMENT
initial_ds: dw DESTINATION_SEGMENT
initial_ss: dw DESTINATION_SEGMENT

start:
	; Setup data segment to 0000
	mov ax, cs
	mov ds, ax

%ifdef TRACE
	; Setup stack
	mov ax, [destination_segment]
	dec ax
	mov ss, ax
	mov sp, 15
%endif

%ifdef INT21H_HELPERS
	; Install our handler at 0000:0084
	push ds
	xor ax, ax
	mov ds, ax
	mov ax, int21
	mov word [21h * 4], ax
	mov ax, cs
	mov word [21h * 4 + 2], ax
	pop ds
%endif

%ifdef TRACE
	; Trace
	mov ah, 00Eh
	mov al, '1'
	mov bh, 0
	mov bl, 1
	mov cx, 1
	int 10h
%endif

	; ES:DI is the destination
	mov ax, [destination_segment]
	mov es, ax
	xor di, di

	mov cx, [sectors_to_copy]

	; DS:SI is the source (CS + 512 to skip this loader)
	mov ax, cs
	add ax, 0x20
	mov ds, ax
	xor si, si

	; multiply by 512 (sector size)
	shl cx, 1
	shl cx, 1
	shl cx, 1
	shl cx, 1
	shl cx, 1
	shl cx, 1
	shl cx, 1
	shl cx, 1
	shl cx, 1

	rep movsb

	; Restore data segment
	mov ax, cs
	mov ds, ax


loading_done:
%ifdef TRACE
	mov ah, 00Eh
	mov al, '2'
	mov bh, 0
	mov bl, 1
	mov cx, 1
	int 10h
%endif

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
	cmp ah, 02h
	je int21_02
	cmp ah, 25h
	je int21_25
	cmp ah, 35h
	je int21_35
	cmp ah, 09h
	je int21_09
	iret

	; AH : 02
	; DL : Character
int21_02:
	push ax
	push bx
	; Int 10,E : write text in tty mode
	; AL : Ascii character
	; BH : Page no
	; BL : Forground color
	xor bx,bx
	mov al, dl
	mov ah, 0x0e
	int 10h
	pop bx
	pop ax
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

	db "Booterify version ",VERSION,0


; Make sure exactly 512 bytes are occupied
section .marker start=511
	db 0xff


section .data
