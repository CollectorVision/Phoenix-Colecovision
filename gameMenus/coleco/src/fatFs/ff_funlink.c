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
/* Delete a File/Directory                                               */
/*-----------------------------------------------------------------------*/

FRESULT f_unlink (
	const TCHAR* path		/* Pointer to the file or directory path */
)
{
	FRESULT res;
	DIR dj, sdj;
	DWORD dclst = 0;
	FATFS *fs;
#if FF_FS_EXFAT
	FFOBJID obj;
#endif
	DEF_NAMBUF


	/* Get logical drive */
#if FF_FS_ONEDRIVE != 1
	res = find_volume(&path, &fs, FA_WRITE);
#else
	res = find_volume(&fs, FA_WRITE);
#endif
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);		/* Follow the file path */
		if (FF_FS_RPATH && res == FR_OK && (dj.fn[NSFLAG] & NS_DOT)) {
			res = FR_INVALID_NAME;			/* Cannot remove dot entry */
		}
#if FF_FS_LOCK != 0
		if (res == FR_OK) res = chk_lock(&dj, 2);	/* Check if it is an open object */
#endif
		if (res == FR_OK) {					/* The object is accessible */
			if (dj.fn[NSFLAG] & NS_NONAME) {
				res = FR_INVALID_NAME;		/* Cannot remove the origin directory */
			} else {
				if (dj.obj.attr & AM_RDO) {
					res = FR_DENIED;		/* Cannot remove R/O object */
				}
			}
			if (res == FR_OK) {
#if FF_FS_EXFAT
				obj.fs = fs;
				if (fs->fs_type == FS_EXFAT) {
					init_alloc_info(fs, &obj);
					dclst = obj.sclust;
				} else
#endif
				{
					dclst = ld_clust(fs, dj.dir);
				}
				if (dj.obj.attr & AM_DIR) {			/* Is it a sub-directory? */
#if FF_FS_RPATH != 0
					if (dclst == fs->cdir) {		 	/* Is it the current directory? */
						res = FR_DENIED;
					} else
#endif
					{
						sdj.obj.fs = fs;				/* Open the sub-directory */
						sdj.obj.sclust = dclst;
#if FF_FS_EXFAT
						if (fs->fs_type == FS_EXFAT) {
							sdj.obj.objsize = obj.objsize;
							sdj.obj.stat = obj.stat;
						}
#endif
						res = dir_sdi(&sdj, 0);
						if (res == FR_OK) {
							res = DIR_READ_FILE(&sdj);			/* Test if the directory is empty */
							if (res == FR_OK) res = FR_DENIED;	/* Not empty? */
							if (res == FR_NO_FILE) res = FR_OK;	/* Empty? */
						}
					}
				}
			}
			if (res == FR_OK) {
				res = dir_remove(&dj);			/* Remove the directory entry */
				if (res == FR_OK && dclst != 0) {	/* Remove the cluster chain if exist */
#if FF_FS_EXFAT
					res = remove_chain(&obj, dclst, 0);
#else
					res = remove_chain(&dj.obj, dclst, 0);
#endif
				}
				if (res == FR_OK) res = sync_fs(fs);
			}
		}
		FREE_NAMBUF();
	}

	LEAVE_FF(fs, res);
}
#endif
