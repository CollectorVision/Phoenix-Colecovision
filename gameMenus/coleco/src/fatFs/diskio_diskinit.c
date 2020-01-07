/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for Phoenix SD Card                         */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "../phoenix.h" /* declarations of Phoenix hardware */

// Per the docs, we need to provide:
// disk_status
// disk_initialize
// disk_read
// (and the unicode stuff for LFNs, which is in ffunicode.c)
// We only have one disk, so we aren't doing anything special with the pdrv args

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

unsigned char csHigh, csLow;
DSTATUS Stat = STA_NOINIT;	/* Disk status */
BYTE CardType;			    /* b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing */

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (
#if FF_FS_ONEDRIVE != 1
	BYTE drv		/* Physical drive nmuber (0) */
#endif
)
{
	BYTE n, ty, cmd, buf[4];
	UINT tmr;
	DSTATUS s;
#if FF_FS_ONEDRIVE != 1
	if (drv) return RES_NOTRDY;
#endif

    // prepare low speed bus configuration
    csHigh = PH_SD_CE_OFF | PH_SD_LOW_SPEED;
    csLow = PH_SD_LOW_SPEED;

	dly_50us(200);			/* 10ms */
	CS_H();		            /* Deselect CS */

	for (n = 10; n; n--) rcvr_mmc(buf, 1);	/* Apply 80 dummy clocks and the card gets ready to receive command */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
        // prepare high speed bus configuration
        csHigh = PH_SD_CE_OFF;
        csLow = 0;
        
        // start the card detection
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
			rcvr_mmc(buf, 4);							/* Get trailing return value of R7 resp */
			if (buf[2] == 0x01 && buf[3] == 0xAA) {		/* The card can work at vdd range of 2.7-3.6V */
				for (tmr = 1000; tmr; tmr--) {			/* Wait for leaving idle state (ACMD41 with HCS bit) */
					if (send_cmd(ACMD41, 1UL << 30) == 0) break;
					dly_50us(20);
				}
				if (tmr && send_cmd(CMD58, 0) == 0) {	/* Check CCS bit in the OCR */
					rcvr_mmc(buf, 4);
					ty = (buf[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 */
				}
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) 	{
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			for (tmr = 1000; tmr; tmr--) {			/* Wait for leaving idle state */
				if (send_cmd(cmd, 0) == 0) break;
				dly_50us(20);
			}
			if (!tmr || send_cmd(CMD16, 512) != 0) {/* Set R/W block length to 512 */
				ty = 0;
            }
		}
	}
	CardType = ty;
	s = ty ? 0 : STA_NOINIT;
	Stat = s;

	deselect();

	return s;
}

