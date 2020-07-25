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

#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Get Number of Free Clusters                                           */
/*-----------------------------------------------------------------------*/

FRESULT f_getfree (
	const TCHAR* path,	/* Logical drive number */
	DWORD* nclst,		/* Pointer to a variable to return number of free clusters */
	FATFS** fatfs		/* Pointer to return pointer to corresponding filesystem object */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD nfree, clst, sect, stat;
	UINT i;
	FFOBJID obj;


	/* Get logical drive */
    res = find_volume(
#if FF_FS_ONEDRIVE != 1
                        &path,      // path, only if not ONEDRIVE
#endif
                        &fs         // fs, always
#if FF_FS_READONLY != 1
                        ,0          // access mode read, if not read-only
#endif
    );

	if (res == FR_OK) {
		*fatfs = fs;				/* Return ptr to the fs object */
		/* If free_clst is valid, return it without full FAT scan */
		if (fs->free_clst <= fs->n_fatent - 2) {
			*nclst = fs->free_clst;
		} else {
			/* Scan FAT to obtain number of free clusters */
			nfree = 0;
			if (fs->fs_type == FS_FAT12) {	/* FAT12: Scan bit field FAT entries */
				clst = 2; obj.fs = fs;
				do {
					stat = get_fat(&obj, clst);
					if (stat == 0xFFFFFFFF) { res = FR_DISK_ERR; break; }
					if (stat == 1) { res = FR_INT_ERR; break; }
					if (stat == 0) nfree++;
				} while (++clst < fs->n_fatent);
			} else {
#if FF_FS_EXFAT
				if (fs->fs_type == FS_EXFAT) {	/* exFAT: Scan allocation bitmap */
					BYTE bm;
					UINT b;

					clst = fs->n_fatent - 2;	/* Number of clusters */
					sect = fs->bitbase;			/* Bitmap sector */
					i = 0;						/* Offset in the sector */
					do {	/* Counts numbuer of bits with zero in the bitmap */
						if (i == 0) {
							res = move_window(fs, sect++);
							if (res != FR_OK) break;
						}
						for (b = 8, bm = fs->win[i]; b && clst; b--, clst--) {
							if (!(bm & 1)) nfree++;
							bm >>= 1;
						}
						i = (i + 1) % SS(fs);
					} while (clst);
				} else
#endif
				{	/* FAT16/32: Scan WORD/DWORD FAT entries */
					clst = fs->n_fatent;	/* Number of entries */
					sect = fs->fatbase;		/* Top of the FAT */
					i = 0;					/* Offset in the sector */
					do {	/* Counts numbuer of entries with zero in the FAT */
						if (i == 0) {
							res = move_window(fs, sect++);
							if (res != FR_OK) break;
						}
						if (fs->fs_type == FS_FAT16) {
							if (ld_word(fs->win + i) == 0) nfree++;
							i += 2;
						} else {
							if ((ld_dword(fs->win + i) & 0x0FFFFFFF) == 0) nfree++;
							i += 4;
						}
						i %= SS(fs);
					} while (--clst);
				}
			}
			*nclst = nfree;			/* Return the free clusters */
			fs->free_clst = nfree;	/* Now free_clst is valid */
			fs->fsi_flag |= 1;		/* FAT32: FSInfo is to be updated */
		}
	}

	LEAVE_FF(fs, res);
}
#endif
