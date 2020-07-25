/*-----------------------------------------------------------------------/
/  Low level disk interface modlue include file   (C)ChaN, 2014          /
/-----------------------------------------------------------------------*/

#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

/* Status of Disk Functions */
typedef BYTE	DSTATUS;

/* Results of Disk Functions */
typedef enum {
	RES_OK = 0,		/* 0: Successful */
	RES_ERROR,		/* 1: R/W Error */
	RES_WRPRT,		/* 2: Write Protected */
	RES_NOTRDY,		/* 3: Not Ready */
	RES_PARERR		/* 4: Invalid Parameter */
} DRESULT;


/*---------------------------------------*/
/* Prototypes for disk control functions */

#if FF_FS_ONEDRIVE != 1
DSTATUS disk_initialize (BYTE pdrv);
DSTATUS disk_status (BYTE pdrv);
DRESULT disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff);
#if !FF_FS_READONLY
DRESULT disk_write (BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
#endif
#else
DSTATUS disk_initialize ();
DSTATUS disk_status ();
DRESULT disk_read (BYTE* buff, DWORD sector, UINT count);
DRESULT disk_ioctl (BYTE cmd, void* buff);
#if !FF_FS_READONLY
DRESULT disk_write (BYTE* buff, DWORD sector, UINT count);
#endif
#endif

// Internal
void xmit_mmc(const BYTE* buff, UINT bc);
void rcvr_mmc(BYTE * buff, UINT bc);
int wait_ready();
void deselect();
int select();
int rcvr_datablock(BYTE *buff, UINT btr);
int xmit_datablock(const BYTE *buff, BYTE token);
BYTE send_cmd(BYTE cmd, DWORD arg);



/* Disk Status Bits (DSTATUS) */

#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */
#define STA_PROTECT		0x04	/* Write protected */


/* Command code for disk_ioctrl fucntion */

/* Generic command (Used by FatFs) */
#define CTRL_SYNC			0	/* Complete pending write process (needed at FF_FS_READONLY == 0) */
#define GET_SECTOR_COUNT	1	/* Get media size (needed at FF_USE_MKFS == 1) */
#define GET_SECTOR_SIZE		2	/* Get sector size (needed at FF_MAX_SS != FF_MIN_SS) */
#define GET_BLOCK_SIZE		3	/* Get erase block size (needed at FF_USE_MKFS == 1) */
#define CTRL_TRIM			4	/* Inform device that the data on the block of sectors is no longer used (needed at FF_USE_TRIM == 1) */

/* Generic command (Not used by FatFs) */
#define CTRL_POWER			5	/* Get/Set power status */
#define CTRL_LOCK			6	/* Lock/Unlock media removal */
#define CTRL_EJECT			7	/* Eject media */
#define CTRL_FORMAT			8	/* Create physical format on the media */

/* MMC/SDC specific ioctl command */
#define MMC_GET_TYPE		10	/* Get card type */
#define MMC_GET_CSD			11	/* Get CSD */
#define MMC_GET_CID			12	/* Get CID */
#define MMC_GET_OCR			13	/* Get OCR */
#define MMC_GET_SDSTAT		14	/* Get SD status */
#define ISDIO_READ			55	/* Read data form SD iSDIO register */
#define ISDIO_WRITE			56	/* Write data to SD iSDIO register */
#define ISDIO_MRITE			57	/* Masked write data to SD iSDIO register */

/* ATA/CF specific ioctl command */
#define ATA_GET_REV			20	/* Get F/W revision */
#define ATA_GET_MODEL		21	/* Get model name */
#define ATA_GET_SN			22	/* Get serial number */

#define	CS_H()		phSDControl=csHigh	/* Set MMC CS "high" */
#define CS_L()		phSDControl=csLow	/* Set MMC CS "low" */

extern unsigned char csHigh, csLow;	/* commands to write for CS control (include speed bits) */
extern DSTATUS Stat;					/* Disk status */
extern BYTE CardType;			    	/* b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing */

/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* MMC/SD command (SPI mode) */
#define CMD0	((BYTE)0)			/* GO_IDLE_STATE */
#define CMD1	((BYTE)1)			/* SEND_OP_COND */
#define	ACMD41	((BYTE)(0x80+41))	/* SEND_OP_COND (SDC) */
#define CMD8	((BYTE)8)			/* SEND_IF_COND */
#define CMD9	((BYTE)9)			/* SEND_CSD */
#define CMD10	((BYTE)10)		/* SEND_CID */
#define CMD12	((BYTE)12)		/* STOP_TRANSMISSION */
#define CMD13	((BYTE)13)		/* SEND_STATUS */
#define ACMD13	((BYTE)(0x80+13))	/* SD_STATUS (SDC) */
#define CMD16	((BYTE)16)		/* SET_BLOCKLEN */
#define CMD17	((BYTE)17)		/* READ_SINGLE_BLOCK */
#define CMD18	((BYTE)18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	((BYTE)23)		/* SET_BLOCK_COUNT */
#define	ACMD23	((BYTE)(0x80+23))	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	((BYTE)24)		/* WRITE_BLOCK */
#define CMD25	((BYTE)25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	((BYTE)32)		/* ERASE_ER_BLK_START */
#define CMD33	((BYTE)33)		/* ERASE_ER_BLK_END */
#define CMD38	((BYTE)38)		/* ERASE */
#define CMD55	((BYTE)55)		/* APP_CMD */
#define CMD58	((BYTE)58)		/* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */

#endif
