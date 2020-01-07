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
/* Send a data packet to the card                                        */
/*-----------------------------------------------------------------------*/
int xmit_datablock (	/* 1:OK, 0:Failed */
	const BYTE *buff,	    /* 512 byte data block to be transmitted */
	BYTE token			    /* Data/Stop token */
)
{
	BYTE d[2];
    
	if (!wait_ready()) return 0;

	d[0] = token;
	xmit_mmc(d, 1);				    /* Xmit a token */
	if (token != 0xFD) {		    /* Is it data token? */
		xmit_mmc(buff, 512);	    /* Xmit the 512 byte data block to MMC */
		rcvr_mmc(d, 2);			    /* Xmit dummy CRC (0xFF,0xFF) */
		rcvr_mmc(d, 1);			    /* Receive data response */
		if ((d[0] & 0x1F) != 0x05)	/* If not accepted, return with error */
			return 0;
	}

	return 1;
}
