#include "vdp.h"

extern void vdpmemcpyfast(unsigned int pAddr, const unsigned char *pSrc, unsigned int cnt);
void dummyfunc() {
__asm
; stack has ret, pAddr, source, count
; for OTIR we need port in C, src in HL and count in B
_vdpmemcpyfast::
; save regs
    push hl
    push bc
    push de
    push af

; point sp at pAddr
    ld hl,#10
    add hl,sp
    ld sp,hl	; sp pointing at pAddr

; set the VDP address
	pop de		; sp pointing at pSrc
	ld a,e
	out (_VDPWA),a
	ld a,d
	set 6,a
	out (_VDPWA),a
	
; get source into hl
	pop hl		; sp pointing at cnt

; get count into de
	pop de		; sp above stack frame

; get port into c
    ld c,#_VDPWD
    	
; outer loop
$0001:
	inc d		; zero test - http://z80-heaven.wikidot.com/optimization#toc14
	dec d
	jr Z,$0002	; if no

; since were > 256 but can only do 256 at a time, just run it that way
	xor a
	ld b,a
	otir		; runs 256
	
	dec d		; subtract 256
	jr $0001
	
; last pass
$0002:
    inc e
    dec e
    jr Z,$0003  ; 0 bytes remaining
	ld b,e		; get remaining count
	otir
	
; restore the stack and return
$0003:
	ld hl,#-16	; pointing at af
    add hl,sp
    ld sp,hl

	pop af
	pop de
	pop bc
	pop hl

__endasm;
}
