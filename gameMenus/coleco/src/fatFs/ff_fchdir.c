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

/*---------------------------------------------------------------------------

   Public Functions (FatFs API) (covered in ff.h)

----------------------------------------------------------------------------*/

#if FF_FS_RPATH >= 1


FRESULT f_chdir (
	const TCHAR* path	/* Pointer to the directory path */
)
{
#if FF_STR_VOLUME_ID == 2
	UINT i;
#endif
	FRESULT res;
	DIR dj;
	FATFS *fs;
	DEF_NAMBUF


	/* Get logical drive */
#if FF_FS_ONEDRIVE != 1
	res = find_volume(&path, &fs, 0);
#else
	res = find_volume(&fs, 0);
#endif
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);		/* Follow the path */
		if (res == FR_OK) {					/* Follow completed */
			if (dj.fn[NSFLAG] & NS_NONAME) {	/* Is it the start directory itself? */
				fs->cdir = dj.obj.sclust;
#if FF_FS_EXFAT
				if (fs->fs_type == FS_EXFAT) {
					fs->cdc_scl = dj.obj.c_scl;
					fs->cdc_size = dj.obj.c_size;
					fs->cdc_ofs = dj.obj.c_ofs;
				}
#endif
			} else {
				if (dj.obj.attr & AM_DIR) {	/* It is a sub-directory */
#if FF_FS_EXFAT
					if (fs->fs_type == FS_EXFAT) {
						fs->cdir = ld_dword(fs->dirbuf + XDIR_FstClus);		/* Sub-directory cluster */
						fs->cdc_scl = dj.obj.sclust;						/* Save containing directory information */
						fs->cdc_size = ((DWORD)dj.obj.objsize & 0xFFFFFF00) | dj.obj.stat;
						fs->cdc_ofs = dj.blk_ofs;
					} else
#endif
					{
						fs->cdir = ld_clust(fs, dj.dir);					/* Sub-directory cluster */
					}
				} else {
					res = FR_NO_PATH;		/* Reached but a file */
				}
			}
		}
		FREE_NAMBUF();
		if (res == FR_NO_FILE) res = FR_NO_PATH;
#if FF_STR_VOLUME_ID == 2	/* Also current drive is changed at Unix style volume ID */
		if (res == FR_OK) {
			for (i = FF_VOLUMES - 1; i && fs != FatFs[i]; i--) ;	/* Set current drive */
			CurrVol = (BYTE)i;
		}
#endif
	}

	LEAVE_FF(fs, res);
}

#endif /* FF_FS_RPATH >= 1 */

