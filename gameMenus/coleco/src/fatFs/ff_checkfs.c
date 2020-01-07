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

/*-----------------------------------------------------------------------*/
/* Load a sector and check if it is an FAT VBR                           */
/*-----------------------------------------------------------------------*/

BYTE check_fs (	/* 0:FAT, 1:exFAT, 2:Valid BS but not FAT, 3:Not a BS, 4:Disk error */
	FATFS* fs,			/* Filesystem object */
	DWORD sect			/* Sector# (lba) to load and check if it is an FAT-VBR or not */
)
{
	fs->wflag = 0; fs->winsect = 0xFFFFFFFF;		/* Invaidate window */
	if (move_window(fs, sect) != FR_OK) {
        return 4;	/* Load boot record */
    }

	if (ld_word(fs->win + BS_55AA) != 0xAA55) {
        return 3;	/* Check boot record signature (always here regardless of the sector size) */
    }

#if FF_FS_EXFAT
	if (!mem_cmp(fs->win + BS_JmpBoot, "\xEB\x76\x90" "EXFAT   ", 11)) return 1;	/* Check if exFAT VBR */
#endif
	if (fs->win[BS_JmpBoot] == 0xE9 || fs->win[BS_JmpBoot] == 0xEB || fs->win[BS_JmpBoot] == 0xE8) {	/* Valid JumpBoot code? */
		if (!mem_cmp(fs->win + BS_FilSysType, "FAT", 3)) {
            return 0;		/* Is it an FAT VBR? */
        }
		if (!mem_cmp(fs->win + BS_FilSysType32, "FAT32", 5)) {
            return 0;	/* Is it an FAT32 VBR? */
        }
	}
	return 2;	/* Valid BS but not FAT */
}

