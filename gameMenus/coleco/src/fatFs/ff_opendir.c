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

#if FF_FS_MINIMIZE <= 2
#if FF_FS_MINIMIZE <= 1

/*-----------------------------------------------------------------------*/
/* Create a Directory Object                                             */
/*-----------------------------------------------------------------------*/

FRESULT f_opendir (
	DIR* dp,			/* Pointer to directory object to create */
	const TCHAR* path	/* Pointer to the directory path */
)
{
	FRESULT res;
	FATFS *fs;
	DEF_NAMBUF

	if (!dp) return FR_INVALID_OBJECT;

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
		dp->obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(dp, path);			/* Follow the path to the directory */
		if (res == FR_OK) {						/* Follow completed */
			if (!(dp->fn[NSFLAG] & NS_NONAME)) {	/* It is not the origin directory itself */
				if (dp->obj.attr & AM_DIR) {		/* This object is a sub-directory */
#if FF_FS_EXFAT
					if (fs->fs_type == FS_EXFAT) {
						dp->obj.c_scl = dp->obj.sclust;							/* Get containing directory inforamation */
						dp->obj.c_size = ((DWORD)dp->obj.objsize & 0xFFFFFF00) | dp->obj.stat;
						dp->obj.c_ofs = dp->blk_ofs;
						init_alloc_info(fs, &dp->obj);	/* Get object allocation info */
					} else
#endif
					{
						dp->obj.sclust = ld_clust(fs, dp->dir);	/* Get object allocation info */
					}
				} else {						/* This object is a file */
					res = FR_NO_PATH;
				}
			}
			if (res == FR_OK) {
				dp->obj.id = fs->id;
				res = dir_sdi(dp, 0);			/* Rewind directory */
#if FF_FS_LOCK != 0
				if (res == FR_OK) {
					if (dp->obj.sclust != 0) {
						dp->obj.lockid = inc_lock(dp, 0);	/* Lock the sub directory */
						if (!dp->obj.lockid) res = FR_TOO_MANY_OPEN_FILES;
					} else {
						dp->obj.lockid = 0;	/* Root directory need not to be locked */
					}
				}
#endif
			}
		}
		FREE_NAMBUF();
		if (res == FR_NO_FILE) res = FR_NO_PATH;
	}
	if (res != FR_OK) dp->obj.fs = 0;		/* Invalidate the directory object if function faild */

	LEAVE_FF(fs, res);
}



#endif /* FF_FS_MINIMIZE <= 1 */
#endif /* FF_FS_MINIMIZE <= 2 */
