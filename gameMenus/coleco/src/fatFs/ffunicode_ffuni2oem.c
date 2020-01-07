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
#ifdef ff_uni2oem
#undef ff_uni2oem
#define ff_uni2oem ff_uni2oem_unused
#endif

WCHAR ff_uni2oem (	/* Returns OEM code character, zero on error */
	DWORD	uni,	/* UTF-16 encoded character to be converted */
	WORD	cp		/* Code page for the conversion */
)
{
	WCHAR c = 0;
	const WCHAR *p = CVTBL(uc, FF_CODE_PAGE);


	if (uni < 0x80) {	/* ASCII? */
		c = (WCHAR)uni;

	} else {			/* Non-ASCII */
		if (uni < 0x10000 && cp == FF_CODE_PAGE) {	/* Is it in BMP and valid code page? */
			for (c = 0; c < 0x80 && uni != p[c]; c++) ;
			c = (c + 0x80) & 0xFF;
		}
	}

	return c;
}


#endif
#endif /* #if FF_USE_LFN */
