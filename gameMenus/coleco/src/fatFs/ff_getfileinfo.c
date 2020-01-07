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
#include "../memset.h"


#if FF_DEFINED != 86604	/* Revision ID */
#error Wrong include file (ff.h).
#endif

/*--------------------------------------------------------------------------

   Module Private Functions (ff_lcl.h)

---------------------------------------------------------------------------*/

#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2
/*-----------------------------------------------------------------------*/
/* Get file information from directory entry                             */
/*-----------------------------------------------------------------------*/

void get_fileinfo (
	DIR* dp,			/* Pointer to the directory object */
	FILINFO* fno		/* Pointer to the file information to be filled */
)
{
#if FF_USE_LFN
#if FF_INLINE_PUT_UTF == 1
    // this assumes Tursi's shorter long filenames
	unsigned char si, di;
    unsigned char wc, hs;
#else
	UINT si, di;
	WCHAR wc, hs;
#endif
	FATFS *fs = dp->obj.fs;
#else
	TCHAR c;
	UINT si, di;
#endif

	fno->fname[0] = 0;			/* Invaidate file info */
	if (dp->sect == 0) return;	/* Exit if read pointer has reached end of directory */

#if FF_USE_LFN		/* LFN configuration */
#if FF_FS_EXFAT
	if (fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
		get_xfileinfo(fs->dirbuf, fno);
		return;
	} else
#endif
	{	/* On the FAT/FAT32 volume */
		if (dp->blk_ofs != 0xFFFFFFFF) {	/* Get LFN if available */
			si = di = hs = 0;
			while (fs->lfnbuf[si] != 0) {
#if FF_INLINE_PUT_UTF == 1
                if (di >= FF_LFN_BUF) { di = 0; break; }
                wc = fs->lfnbuf[si++];
                fno->fname[di++] = wc;
                if (wc == 0) {di = 0; break; }
#else
				wc = fs->lfnbuf[si++];		/* Get an LFN character (UTF-16) */
				if (hs == 0 && IsSurrogate(wc)) {	/* Is it a surrogate? */
					hs = wc; continue;		/* Get low surrogate */
				}
				wc = put_utf((DWORD)hs << 16 | wc, &fno->fname[di], FF_LFN_BUF - di);	/* Store it in UTF-16 or UTF-8 encoding */
				if (wc == 0) { di = 0; break; }	/* Invalid char or buffer overflow? */
				di += wc;
#endif
				hs = 0;
			}
			if (hs != 0) di = 0;	/* Broken surrogate pair? */
			fno->fname[di] = 0;		/* Terminate the LFN (null string means LFN is invalid) */
		}
	}

	si = di = 0;
#if FF_FS_REMOVE_ALTNAME == 1
    if (fno->fname[0] == 0) {
#endif
	while (si < 11) {		/* Get SFN from SFN entry */

		wc = dp->dir[si++];			/* Get a char */
		if (wc == ' ') continue;	/* Skip padding spaces */
		if (wc == RDDEM) wc = DDEM;	/* Restore replaced DDEM character */
#if FF_FS_REMOVE_ALTNAME == 1
		if (si == 9 && di < FF_SFN_BUF) fno->fname[di++] = (TCHAR)'.';	/* Insert a . if extension is exist */
#else
		if (si == 9 && di < FF_SFN_BUF) fno->altname[di++] = '.';	/* Insert a . if extension is exist */
#endif
#if FF_LFN_UNICODE >= 1	/* Unicode output */
		if (dbc_1st((BYTE)wc) && si != 8 && si != 11 && dbc_2nd(dp->dir[si])) {	/* Make a DBC if needed */
			wc = wc << 8 | dp->dir[si++];
		}
		wc = ff_oem2uni(wc, CODEPAGE);		/* ANSI/OEM -> Unicode */
		if (wc == 0) { di = 0; break; }		/* Wrong char in the current code page? */
#if FF_FS_REMOVE_ALTNAME == 1
        // todo: #if FF_INLINE_PUT_UTF == 1
		wc = put_utf(wc, &fno->fname[di], FF_SFN_BUF - di);	/* Store it in Unicode */
#else
        // todo: #if FF_INLINE_PUT_UTF == 1
        wc = put_utf(wc, &fno->altname[di], FF_SFN_BUF - di);	/* Store it in Unicode */
#endif
		if (wc == 0) { di = 0; break; }		/* Buffer overflow? */
		di += wc;
#else					/* ANSI/OEM output */
#if FF_FS_REMOVE_ALTNAME == 1
		fno->fname[di++] = (TCHAR)wc;	/* Store it without any conversion */
#else
		fno->altname[di++] = (TCHAR)wc;	/* Store it without any conversion */
#endif
#endif
	}
#if FF_FS_REMOVE_ALTNAME == 1
	fno->fname[di] = 0;	/* Terminate the SFN  (null string means SFN is invalid) */
    }   // if fname[0] == 0
#else
	fno->altname[di] = 0;	/* Terminate the SFN  (null string means SFN is invalid) */
#endif

	if (fno->fname[0] == 0) {	/* If LFN is invalid, altname[] needs to be copied to fname[] */
		if (di == 0) {	/* If LFN and SFN both are invalid, this object is inaccesible */
			fno->fname[di++] = '?';
#if FF_FS_REMOVE_ALTNAME != 1
        } else {
			for (si = di = 0; fno->altname[si]; si++, di++) {	/* Copy altname[] to fname[] with case information */
				wc = (WCHAR)fno->altname[si];
#if FF_FS_CASESENSITIVE != 1
				if (IsUpper(wc) && (dp->dir[DIR_NTres] & ((si >= 9) ? NS_EXT : NS_BODY))) wc += 0x20;
#endif
				fno->fname[di] = (TCHAR)wc;
			}
#endif
		}
		fno->fname[di] = 0;	/* Terminate the LFN */
#if FF_FS_REMOVE_ALTNAME != 1
		if (!dp->dir[DIR_NTres]) fno->altname[0] = 0;	/* Altname is not needed if neither LFN nor case info is exist. */
#endif
	}

#else	/* Non-LFN configuration */
	si = di = 0;
	while (si < 11) {		/* Copy name body and extension */
		c = (TCHAR)dp->dir[si++];
		if (c == ' ') continue;		/* Skip padding spaces */
		if (c == RDDEM) c = DDEM;	/* Restore replaced DDEM character */
		if (si == 9) fno->fname[di++] = '.';/* Insert a . if extension is exist */
		fno->fname[di++] = c;
	}
	fno->fname[di] = 0;
#endif

	fno->fattrib = dp->dir[DIR_Attr];					/* Attribute */
	fno->fsize = ld_dword(dp->dir + DIR_FileSize);		/* Size */
#if FF_FS_IGNORE_TIMESTAMP != 1
	fno->ftime = ld_word(dp->dir + DIR_ModTime + 0);	/* Time */
	fno->fdate = ld_word(dp->dir + DIR_ModTime + 2);	/* Date */
#endif
}

#endif /* FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2 */

