/*----------------------------------------------------------------------------/
/  FatFs - Generic FAT Filesystem Module  R0.13c                              /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2018, ChaN, all right reserved.
/
/ FatFs module is an open source software. Redistribution and use of FatFs in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/
/----------------------------------------------------------------------------*/

// heavily modified by Tursi

#include "ff.h"			/* Declarations of FatFs API */
#include "ff_lcl.h"
#include "diskio.h"		/* Declarations of device I/O functions */

#if FF_DEFINED != 86604	/* Revision ID */
#error Wrong include file (ff.h).
#endif

/* Remark: Variables defined here without initial value shall be guaranteed
/  zero/null at start-up. If not, the linker option or start-up routine is
/  not compliance with C standard. */

/*--------------------------------*/
/* File/Volume controls           */
/*--------------------------------*/

#if FF_VOLUMES < 1 || FF_VOLUMES > 10
#error Wrong FF_VOLUMES setting
#endif
FATFS* FatFs[FF_VOLUMES];	/* Pointer to the filesystem objects (logical drives) */
WORD Fsid;					/* Filesystem mount ID */

#if FF_FS_RPATH != 0
BYTE CurrVol;				/* Current drive */
#endif

#if FF_FS_LOCK != 0
FILESEM Files[FF_FS_LOCK];	/* Open object lock semaphores */
#endif

#if FF_STR_VOLUME_ID
#ifdef FF_VOLUME_STRS
const char* const VolumeStr[FF_VOLUMES] = {FF_VOLUME_STRS};	/* Pre-defined volume ID */
#endif
#endif


/*--------------------------------*/
/* LFN/Directory working buffer   */
/*--------------------------------*/

#if FF_USE_LFN == 0		/* Non-LFN configuration */
#if FF_FS_EXFAT
#error LFN must be enabled when enable exFAT
#endif
#define DEF_NAMBUF
#define INIT_NAMBUF(fs)
#define FREE_NAMBUF()
#define LEAVE_MKFS(res)	return res

#else					/* LFN configurations */
#if FF_MAX_LFN < 12 || FF_MAX_LFN > 255
#error Wrong setting of FF_MAX_LFN
#endif
#if FF_LFN_BUF < FF_SFN_BUF || FF_SFN_BUF < 12
#error Wrong setting of FF_LFN_BUF or FF_SFN_BUF
#endif
#if FF_LFN_UNICODE < 0 || FF_LFN_UNICODE > 3
#error Wrong setting of FF_LFN_UNICODE
#endif
const BYTE LfnOfs[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};	/* FAT: Offset of LFN characters in the directory entry */
#define MAXDIRB(nc)	((nc + 44U) / 15 * SZDIRE)	/* exFAT: Size of directory entry block scratchpad buffer needed for the name length */

#if FF_USE_LFN == 1		/* LFN enabled with working buffer */
#if FF_FS_EXFAT
BYTE	DirBuf[MAXDIRB(FF_MAX_LFN)];	/* Directory entry block scratchpad buffer */
#endif
WCHAR LfnBuf[FF_MAX_LFN + 1];		/* LFN working buffer */
#define DEF_NAMBUF
#define INIT_NAMBUF(fs)
#define FREE_NAMBUF()
#define LEAVE_MKFS(res)	return res

#elif FF_USE_LFN == 2 	/* LFN enabled with dynamic working buffer on the stack */
#if FF_FS_EXFAT
#define DEF_NAMBUF		WCHAR lbuf[FF_MAX_LFN+1]; BYTE dbuf[MAXDIRB(FF_MAX_LFN)];	/* LFN working buffer and directory entry block scratchpad buffer */
#define INIT_NAMBUF(fs)	{ (fs)->lfnbuf = lbuf; (fs)->dirbuf = dbuf; }
#define FREE_NAMBUF()
#else
#define DEF_NAMBUF		WCHAR lbuf[FF_MAX_LFN+1];	/* LFN working buffer */
#define INIT_NAMBUF(fs)	{ (fs)->lfnbuf = lbuf; }
#define FREE_NAMBUF()
#endif
#define LEAVE_MKFS(res)	return res

#elif FF_USE_LFN == 3 	/* LFN enabled with dynamic working buffer on the heap */
#if FF_FS_EXFAT
#define DEF_NAMBUF		WCHAR *lfn;	/* Pointer to LFN working buffer and directory entry block scratchpad buffer */
#define INIT_NAMBUF(fs)	{ lfn = ff_memalloc((FF_MAX_LFN+1)*2 + MAXDIRB(FF_MAX_LFN)); if (!lfn) LEAVE_FF(fs, FR_NOT_ENOUGH_CORE); (fs)->lfnbuf = lfn; (fs)->dirbuf = (BYTE*)(lfn+FF_MAX_LFN+1); }
#define FREE_NAMBUF()	ff_memfree(lfn)
#else
#define DEF_NAMBUF		WCHAR *lfn;	/* Pointer to LFN working buffer */
#define INIT_NAMBUF(fs)	{ lfn = ff_memalloc((FF_MAX_LFN+1)*2); if (!lfn) LEAVE_FF(fs, FR_NOT_ENOUGH_CORE); (fs)->lfnbuf = lfn; }
#define FREE_NAMBUF()	ff_memfree(lfn)
#endif
#define LEAVE_MKFS(res)	{ if (!work) ff_memfree(buf); return res; }
#define MAX_MALLOC	0x8000	/* Must be >=FF_MAX_SS */

#else
#error Wrong setting of FF_USE_LFN

#endif	/* FF_USE_LFN == 1 */
#endif	/* FF_USE_LFN == 0 */



/*--------------------------------*/
/* Code conversion tables         */
/*--------------------------------*/

#if FF_CODE_PAGE == 0		/* Run-time code page configuration */
#define CODEPAGE CodePage
WORD CodePage;	/* Current code page */
const BYTE *ExCvt, *DbcTbl;	/* Pointer to current SBCS up-case table and DBCS code range table below */

const BYTE Ct437[] = TBL_CT437;
const BYTE Ct720[] = TBL_CT720;
const BYTE Ct737[] = TBL_CT737;
const BYTE Ct771[] = TBL_CT771;
const BYTE Ct775[] = TBL_CT775;
const BYTE Ct850[] = TBL_CT850;
const BYTE Ct852[] = TBL_CT852;
const BYTE Ct855[] = TBL_CT855;
const BYTE Ct857[] = TBL_CT857;
const BYTE Ct860[] = TBL_CT860;
const BYTE Ct861[] = TBL_CT861;
const BYTE Ct862[] = TBL_CT862;
const BYTE Ct863[] = TBL_CT863;
const BYTE Ct864[] = TBL_CT864;
const BYTE Ct865[] = TBL_CT865;
const BYTE Ct866[] = TBL_CT866;
const BYTE Ct869[] = TBL_CT869;
const BYTE Dc932[] = TBL_DC932;
const BYTE Dc936[] = TBL_DC936;
const BYTE Dc949[] = TBL_DC949;
const BYTE Dc950[] = TBL_DC950;

#elif FF_CODE_PAGE < 900	/* code page configuration (SBCS) */
#define CODEPAGE FF_CODE_PAGE
const BYTE ExCvt[] = MKCVTBL(TBL_CT, FF_CODE_PAGE);

#else					/* code page configuration (DBCS) */
#define CODEPAGE FF_CODE_PAGE
const BYTE DbcTbl[] = MKCVTBL(TBL_DC, FF_CODE_PAGE);

#endif
