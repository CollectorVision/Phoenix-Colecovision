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

FRESULT move_window (	/* Returns FR_OK or FR_DISK_ERR */
	FATFS* fs,			/* Filesystem object */
	DWORD sector		/* Sector number to make appearance in the fs->win[] */
)
{
	FRESULT res = FR_OK;

	if (sector != fs->winsect) {	/* Window offset changed? */
#if !FF_FS_READONLY
		res = sync_window(fs);		/* Write-back changes */
#endif
		if (res == FR_OK) {			/* Fill sector window with new data */
#if FF_FS_ONEDRIVE != 1
			if (disk_read(fs->pdrv, fs->win, sector, 1) != RES_OK) {
#else
			if (disk_read(fs->win, sector, 1) != RES_OK) {
#endif
				sector = 0xFFFFFFFF;	/* Invalidate window if read data is not valid */
				res = FR_DISK_ERR;
			}
			fs->winsect = sector;
		}
	}
	return res;
}

