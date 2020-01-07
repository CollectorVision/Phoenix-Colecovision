/*------------------------------------------------------------------------*/
/* Unicode handling functions for FatFs R0.13c                            */
/*------------------------------------------------------------------------*/
/* This module will occupy a huge memory in the .const section when the    /
/  FatFs is configured for LFN with DBCS. If the system has any Unicode    /
/  utilitiy for the code conversion, this module should be modified to use /
/  that function to avoid silly memory consumption.                        /
/-------------------------------------------------------------------------*/
/*
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
*/
 

#include "ff.h"

#if FF_USE_LFN	/* This module will be blanked at non-LFN configuration */

#if FF_DEFINED != 86604	/* Revision ID */
#error Wrong include file (ff.h).
#endif

#define MERGE2(a, b) a ## b
#define CVTBL(tbl, cp) MERGE2(tbl, cp)

extern const WCHAR uc437[];


/*------------------------------------------------------------------------*/
/* OEM <==> Unicode conversions for static code page configuration        */
/* SBCS fixed code page                                                   */
/*------------------------------------------------------------------------*/

#if FF_CODE_PAGE != 0 && FF_CODE_PAGE < 900

// Tursi: undo the deletion hack
#ifdef ff_oem2uni
#undef ff_oem2uni
#define ff_oem2uni ff_oem2uni_unused
#endif


WCHAR ff_oem2uni (	/* Returns Unicode character, zero on error */
	WCHAR	oem,	/* OEM code to be converted */
	WORD	cp		/* Code page for the conversion */
)
{
	WCHAR c = 0;
	const WCHAR *p = CVTBL(uc, FF_CODE_PAGE);


	if (oem < 0x80) {	/* ASCII? */
		c = oem;

	} else {			/* Extended char */
		if (cp == FF_CODE_PAGE) {	/* Is it a valid code page? */
			if (oem < 0x100) c = p[oem - 0x80];
		}
	}

	return c;
}

#endif

#endif /* #if FF_USE_LFN */
