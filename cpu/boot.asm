org 0xa000

screen		equ	0x4000
pixels_len	equ	6144
attributes	equ	0x5800
attributes_len	equ	768

; variables
cursor		equ	0x5b00
stack		equ	0x5c00

main proc

	di
	ld	hl, stack
	ld	sp, stack

; clear screen
	ld	hl, screen
	ld	(cursor), hl
	ld	de, screen + 1
	ld	bc, pixels_len
	xor	a
	ld	(hl), a
	ldir
; set attributes (black on white)
	ld	bc, 512
	ld	a, 0x38
	ld	(hl), a
	ldir

; paint an RGB strip in the lower third
	ld	b, 64
	xor	a
_rgbfill:
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	add	a, 0x08
	and	0x7f
	djnz	_rgbfill

; write a banner
	ld	de, banner
	call	puts

; and, cut.
	ld	a, 4
	out	(0xfe), a
	di
	halt

puts:
	; if (!*de) return;
	ld	a, (de)
	or	a
	ret	z
	; putchar(*de++)
	inc	de
	call	putchar
	jr	puts

putchar:
	exx
	; hl = a * 4
	ld	l, a
	ld	h, 0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	; hl += &font - 256   (font data starts with '@')
	ld	de, font - 256
	add	hl, de
	; read cursor location, and advance the cursor
	ld	de, (cursor)
	inc	de
	ld	(cursor), de
	dec	de
	ld	b, 8
_putchar_byte:
	; copy one horizontal byte for the character
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	d
	djnz	_putchar_byte
	exx
	ret


banner:
	db "Hello world", 0

font:
	incbin "clairsys.bin"
font_end:

endp
end main
