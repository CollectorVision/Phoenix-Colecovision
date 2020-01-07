/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for Phoenix SD Card                         */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "../phoenix.h" /* declarations of Phoenix hardware */

// Code adapted from the generic sdmm.c - not updated to Matt's reference just yet
// Obviously not bit-banging though.
// License for those parts:
/*------------------------------------------------------------------------/
/  Foolproof MMCv3/SDv1/SDv2 (in SPI mode) control module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2013, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
*/
/*-----------------------------------------------------------------------*/
/* Receive bytes from the card (hardware)                                */
/*-----------------------------------------------------------------------*/

#if 0
void rcvr_mmc (
	BYTE *buff,	/* Pointer to read buffer */
	UINT bc		/* Number of bytes to receive */
)
{
    // I think the hardware sends the 0xff for us...?
	do {
        *buff++ = phSDData;
	} while (--bc);
}
#else
// these dummyfuncs just make SDCC happy, they resolve to an empty label
void rcvr_mmc_dummy() {
__asm
; stack has ret, buff, bc
; for INIR we need port in C, dest in HL and count in B
_rcvr_mmc::
; save regs
    push hl
    push bc
    push de
    push af

; point sp at buf
    ld hl,#10
    add hl,sp
    ld sp,hl	; sp pointing at buf

; get dest into hl
	pop hl		; sp pointing at cnt

; get count into de
	pop de		; sp above stack frame

; get port into c
    ld c,#_phSDData
    	
; outer loop
$0001:
	inc d		; zero test - http://z80-heaven.wikidot.com/optimization#toc14
	dec d
	jr Z,$0002	; if no

; since were > 256 but can only do 256 at a time, just run it that way
	xor a
	ld b,a
	inir		; runs 256
	
	dec d		; subtract 256
	jr $0001
	
; last pass
$0002:
    inc e
    dec e
    jr Z,$0003  ; 0 bytes remaining
	ld b,e		; get remaining count
	inir
	
; restore the stack and return
$0003:
	ld hl,#-14	; pointing at af
    add hl,sp
    ld sp,hl

	pop af
	pop de
	pop bc
	pop hl
__endasm;
}
#endif

