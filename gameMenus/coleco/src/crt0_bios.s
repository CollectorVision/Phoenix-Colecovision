; crt0.s for Phoenix ColecoVision Boot BIOS
; This is the actual BIOS version

	.module crt0
	.globl _main
	.globl _vdpinit
	.globl _my_nmi
	.globl _vdpLimi
    .globl  l__INITIALIZER
    .globl  s__INITIALIZED
    .globl  s__INITIALIZER

	.area _HEADER(ABS)
	.org 0x0000

; Boot starts here
    ld sp,#0x8000           ; set stack (unnecessarily)
    jp start                ; and jump to the startup code
    .db 0xff, 0xff          ; padding

; These are vectors into the cartridge, normally.
; But, we dont actually WANT to run a cartridge, so we
; will just jump to a dummy stub function that does nothing.
; We swap out to the real BIOS before we boot, so that
; wont even matter. Note these are location sensitive,
; so dont change the code without verifying addresses.

RST_8H:
    JP dummyIRQ
    .db 0xff, 0xff, 0xff, 0xff, 0xff

RST_10H:
    JP dummyIRQ
    .db 0xff, 0xff, 0xff, 0xff, 0xff

RST_18H:
    JP dummyIRQ
    .db 0xff, 0xff, 0xff, 0xff, 0xff

RST_20H:
    JP dummyIRQ
    .db 0xff, 0xff, 0xff, 0xff, 0xff

RST_28H:
    JP dummyIRQ
    .db 0xff, 0xff, 0xff, 0xff, 0xff

RST_30H:
    JP dummyIRQ
    .db 0xff, 0xff, 0xff, 0xff, 0xff

dummyIRQ:
    ei
    reti
    .db 0xff, 0xff, 0xff, 0xff, 0xff

; Just some random noise to pad
    .ascii 'Deae Lunae: Benedicite noctes decora..'

nmi:
    jp run_nmi

;;;;;;;;; BIOS START ;;;;;;;;;;;;
start:
	; clear interrupt flag right off
	ld hl,#_vdpLimi
	ld (hl),#0
	
	; clear RAM before starting
	ld hl,#0x7000			; set copy source
	ld de,#0x7001			; set copy dest
	ld bc,#0x1fff			; set bytes to copy (1 less than size)
	ld (hl),#0				; set initial value (this gets copied through)
	ldir					; do it

    ; we will have to fix the memory bank before we prepare the system RAM
	ld  sp, #0x8000			; 6000-6FFF is reserved for Coleco, 7000-7FFF is used by menu
	call gsinit				; Initialize global variables
	
	call _vdpinit			; Initialize VDP and sound
	
	call _main				; call the user code
	rst 0x0					; Restart when main() returns

	;; Ordering of segments for the linker - copied from sdcc crt0.s
	.area _HOME
	.area _CODE
	.area   _INITIALIZER
	.area   _GSINIT
	.area   _GSFINAL
        
	.area _DATA
	.area _BSS
	.area _HEAP

	.area _BSS
_vdpLimi:		; 0x80 - interrupt set, other bits used by library
	.ds 1

;;;;;;;;;;;;;; Actual NMI handler, same as my lib uses ;;;;;;;;;;;;;;;;;
    .area _CODE
run_nmi:
; all we do is set the MSB of _vdpLimi, and then check
; if the LSB is set. If so, we call user code now. if
; not, the library will deal with it when enabled.
	push af					; save flags (none affected, but play safe!)
	push hl
	
	ld hl,#_vdpLimi
	bit 0,(hl)				; check LSb (enable)
	jp z,notokay
	
; okay, full on call, save off the (other) regs
	push bc
	push de
	;push ix ; saved by callee
	push iy
	call _my_nmi			; call the lib version
	pop iy
	;pop ix
	pop de
	pop bc	
	jp clrup				

notokay:
	set 7,(hl)				; set MSb (flag)

clrup:
	pop hl					
	pop af
	retn

;------------------------------------
; Some basic string functions, here cause
; it is already an asm file...
;------------------------------------

; void *memcpy(void *dest, const void *source, unsigned int count)
; stack has return, dest, src, count
; for LDIR, we need destination in DE, src in HL, and count in BC
_memcpy::
    ld hl,#2
    add hl,sp
    ld sp,hl    ; stack ponts at dest

    pop de      ; get dest
    pop hl      ; get src
    pop bc      ; get count

    ld a,c      ; check count for zero
    or a,b
    jr Z,$mcp1

	ldir

$mcp1:
    ld hl,#-8
    add hl,sp
    ld sp,hl

	ret

;-------------------------------------------------

; void *memset(void *dest, unsigned int val, unsigned int count)
; stack has return, dest, val, count
_memset::
    ld hl,#2
    add hl,sp
    ld sp,hl    ; stack points at dest

;:7: unsigned char *work = buf;
    pop hl          ; hl = pointer
    pop de          ; e = ch
    pop bc          ; bc = count

;:9: while (cnt&0x03) {
$0001:
	ld	a,c
	and	a, #0x03
	jr	Z,$0013

;:10: *(work++) = ch;
	ld	(hl),e
	inc	hl

;:11: --cnt;
	dec	bc
	jr	$0001

;:14: while (cnt) {
$0013:
$0004:
	ld	a,b
	or	a,c
	jr	Z,$0006

;:15: *(work++) = ch;
;:16: *(work++) = ch;
;:17: *(work++) = ch;
;:18: *(work++) = ch;
	ld	(hl),e
	inc	hl
	ld	(hl),e
	inc	hl
	ld	(hl),e
	inc	hl
	ld	(hl),e
	inc	hl

;:19: cnt -= 4;
	ld	a,c
	add	a,#0xFC
	ld	c,a
	ld	a,b
	adc	a,#0xFF
	ld	b,a
	jr	$0004

$0006:
;:22: return buf ;
;	ld	l,4 (ix)
;	ld	h,5 (ix)

    ld hl,#-8
    add hl,sp
    ld sp,hl

	ret

;-------------------------------------------------

; void *memmove(void *dest, void *src, unsigned int count)
; stack has return, dest, src, count
; for LDIR/LDDR, we need destination in DE, src in HL, and count in BC
_memmove::
    ld hl,#2
    add hl,sp
    ld sp,hl    ; stack points at dest

    pop de       ; dest in de
    pop hl       ; src in hl
    pop bc       ; count in bc

    ld a,c       ; test bc for zero
    or a,b
    jp Z,00109$

;:46: if (((int)src < (int)dst) && ((((int)src) + cnt) > (int)dst))
	ld	a,l
	sub	a,e
	ld	a,h
	sbc	a,d
	jp	PO, 00139$  ; src-dest, jump if overflow (ie: src is greater)
	xor	a, #0x80
00139$:
	jp	P,00108$    ; change/check sign, jump if became positive... was negative (ie: not zero)

	push hl         ; this will overwrite count, bc is our only copy!
    ld	a,c
	add	a,l
	ld	l,a
	ld	a,b
	adc	a,h
	ld	h,a         ; src=src+cnt

	ld	a,e
	sub	a,l
	ld	a,d
	sbc	a,h         ; src-dest, jump if no carry (ie: dest is greater)
	jr	NC,00110$

; decrementing, memmove case
;:49: d = ((char *)dst) + cnt - 1;
; need to add bc and decrement de
    ld a,c
    add a,e
    ld e,a
    ld a,b
    adc a,d
    ld d,a          ; dest = dest + cnt
	dec	de

;:50: s = ((char *)src) + cnt - 1;
; src (hl) just needs the -1 now...
	dec	hl

;:51: while (cnt--) {
;:52: *d-- = *s--;
    lddr
    pop hl          ; just to fix up the stack
	jr	00109$
	
; need to recover original hl before we do the following
00110$:
	pop hl

; Incrementing - memcpy case
; all registers needed are still intact
00108$:
;:56: d = dst;
;:57: s = src;

;:58: while (cnt--) {
;:59: *d++ = *s++;
    ldir

; exit
00109$:
;:63: return dst;
;	ld	l,4 (ix)
;	ld	h,5 (ix)
	
    ld hl,#-8
    add hl,sp
    ld sp,hl

	ret

;-------------------------------------------------

	.area _GSINIT
gsinit::
    ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c                    ; size of copy in bc
	jp	z,_main         
	ld	de, #s__INITIALIZED     ; target address
	ld	hl, #s__INITIALIZER     ; source address
	ldir
	.area _GSFINAL
	ret
	
	.area _INITIALIZER

