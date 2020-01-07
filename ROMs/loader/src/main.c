// INCLUDES ////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "joy.h"
#include "vdp.h"
#include "mmc.h"
#include "fat.h"

// DEFINES /////////////////////////////////////////////////////////////////////

#define peek16(A)  (*(volatile unsigned int*)(A))
#define poke16(A,V) *(volatile unsigned int*)(A)=(V)

#define nCS   0x01
#define  SCLK 0x02
#define  MOSI 0x04
#define nWP   0x08
#define nHOLD 0x10

#define  CS   0x00
#define nSCLK 0x00
#define nMOSI 0x00
#define  WP   0x00
#define  HOLD 0x00

#define FI_32KB_BLOCK_ERASE                          0x52
#define FI_64KB_BLOCK_ERASE                          0xD8
#define FI_PAGE_PROGRAM                              0x02
#define FI_READ_DATA                                 0x03
#define FI_READ_STATUS_REGISTER_1                    0x05
#define FI_READ_STATUS_REGISTER_2                    0x35
#define FI_READ_STATUS_REGISTER_3                    0x15
#define FI_SECTOR_ERASE                              0x20
#define FI_WRITE_DISABLE                             0x04
#define FI_WRITE_ENABLE                              0x06
#define FI_WRITE_ENABLE_FOR_VOLATILE_STATUS_REGISTER 0x50
#define FI_WRITE_STATUS_REGISTER_1                   0x01
#define FI_WRITE_STATUS_REGISTER_2                   0x31
#define FI_WRITE_STATUS_REGISTER_3                   0x11

// I/O PORTS ///////////////////////////////////////////////////////////////////

__sfr __at 0x55 CONFIG;
__sfr __at 0x56 STATUS;
__sfr __at 0x58 MACHID;
__sfr __at 0x59 FLASH ;

// CONSTANTS ///////////////////////////////////////////////////////////////////

static const char biosfiles[3][12] =
{
	"COLECO  BIO",
	"ONYX    BIO",
	"SPLICE  BIO"
};
//                                      11111111112222222222333
//                             12345678901234567890123456789012
static const char TITULO[]  = "      PHOENIX FIRMWARE A05      ";
static const char *mcfile   = "MULTCARTROM";
static const char *corefile = "CORE00  PHX";

// GLOBALS /////////////////////////////////////////////////////////////////////

unsigned char *pbios = (unsigned char *)0x0000;

static unsigned char mach_id;

static unsigned char byte = 0xC5;
static char bcd[3];
char msg[32];

// BYTE TO BCD /////////////////////////////////////////////////////////////////

void byte2bcd(void)
{
	unsigned char b;

	b = (byte >> 4) & 0x0F;
	bcd[0] = b + ( (b <= 9) ? '0' : 'A'-10);
	b = byte & 0x0F;
	bcd[1] = b + ( (b <= 9) ? '0' : 'A'-10);
	bcd[2] = '\0';
}

// CENTERED PRINT //////////////////////////////////////////////////////////////

void printCenter(unsigned char y, unsigned char *msg)
{
	unsigned char x;

	x = 16 - strlen(msg) / 2;
	vdp_gotoxy(x, y);
	vdp_putstring(msg);
}

// ERROR ///////////////////////////////////////////////////////////////////////

void erro(unsigned char *erro)
{
	DisableCard();
	vdp_setcolor(COLOR_RED, COLOR_BLACK, COLOR_WHITE);
	printCenter(12, erro);
	for (;;);
}

// START MULTICART /////////////////////////////////////////////////////////////

void startMulticart()
{
	unsigned char *cp = (unsigned char *)0x7100;

//	Disable loader and start BIOS

	*cp++ = 0x3E;		// LD   A,2
	*cp++ = 0x02;
	*cp++ = 0xD3;		// OUT  (CONFIG),A
	*cp++ = 0x55;
	*cp++ = 0xC3;		// JP   0
	*cp++ = 0x00;
	*cp++ = 0x00;

	__asm__("jp 0x7100");
}

// START EXTERNAL CARTRIDGE ////////////////////////////////////////////////////

void startExtCart()
{
	unsigned char *cp = (unsigned char *)0x7100;

//	Disable loader and start external cartridge

	*cp++ = 0x3E;		// LD   A,4
	*cp++ = 0x04;
	*cp++ = 0xD3;		// OUT  (CONFIG),A
	*cp++ = 0x55;
	*cp++ = 0xC3;		// JP   0
	*cp++ = 0x00;
	*cp++ = 0x00;

	__asm__("jp 0x7100");
}

// FLASH INTERFACE WRITE/READ //////////////////////////////////////////////////

unsigned char fiWR(unsigned char ir, unsigned char wc, unsigned char rc, unsigned char h, unsigned char m, unsigned char l, unsigned char d)
{
	signed char i;
	int j;
	unsigned char bi;

//	MODE 0 (DRIVE CLOCK LOW WHILE NOT CS) & WP

	FLASH = nHOLD |  WP | nMOSI | nSCLK | nCS;

//	ASSERT CS

	FLASH = nHOLD |  WP | nMOSI | nSCLK |  CS;

//	1ST BYTE (INSTRUCTION)

	for (i = 7; i >= 0; i--)
	{
		bi = ((ir >> i) & 1) << 2;
		FLASH = nHOLD |  WP | bi    | nSCLK |  CS;
		FLASH = nHOLD |  WP | bi    |  SCLK |  CS;
	}

//	2ND BYTE

	if (wc >= 1)
	for (i = 7; i >= 0; i--)
	{
		bi = ((h >> i) & 1) << 2;
		FLASH = nHOLD |  WP | bi    | nSCLK |  CS;
		FLASH = nHOLD |  WP | bi    |  SCLK |  CS;
	}

//	3RD BYTE

	if (wc >= 2)
	for (i = 7; i >= 0; i--)
	{
		bi = ((m >> i) & 1) << 2;
		FLASH = nHOLD |  WP | bi    | nSCLK |  CS;
		FLASH = nHOLD |  WP | bi    |  SCLK |  CS;
	}

//	4TH BYTE

	if (wc >= 3)
	for (i = 7; i >= 0; i--)
	{
		bi = ((l >> i) & 1) << 2;
		FLASH = nHOLD |  WP | bi    | nSCLK |  CS;
		FLASH = nHOLD |  WP | bi    |  SCLK |  CS;
	}

//	5TH BYTE

	if (wc == 4)
	{
		for (i = 7; i >= 0; i--)
		{
			bi = ((d >> i) & 1) << 2;
			FLASH = nHOLD |  WP | bi    | nSCLK |  CS;
			FLASH = nHOLD |  WP | bi    |  SCLK |  CS;
		}
	}
	
	if (wc == 255)
	{
		for (j = 0; j < 256; j++)
		{
			for (i = 7; i >= 0; i--)
			{
				bi = ((pbios[j] >> i) & 1) << 2;
				FLASH = nHOLD |  WP | bi    | nSCLK |  CS;
				FLASH = nHOLD |  WP | bi    |  SCLK |  CS;
			}
		}
	}

	if (rc == 1)
	{
		bi = 0;
		for (i = 7; i >= 0; i--)
		{
			FLASH = nHOLD |  WP | nMOSI | nSCLK |  CS;
			bi <<= 1;
			bi |= FLASH & 1;
			if (i) FLASH = nHOLD |  WP | nMOSI |  SCLK |  CS;
		}
	}

	FLASH = nHOLD |  WP | bi | nSCLK |  CS;
	FLASH = nHOLD |  WP | bi | nSCLK | nCS;

	return bi;
}

// FLASH INTERFACE WAIT ////////////////////////////////////////////////////////

void fiWait(void)
{
	unsigned char sr1;

	do
	{
		sr1 = fiWR(FI_READ_STATUS_REGISTER_1, 0, 1, 0, 0, 0, 0);
	}
	while (sr1 & 1);
}

// DUMP STRING + BCD ///////////////////////////////////////////////////////////

void dump(int row, char *str)
{
	byte2bcd();
	strcpy(msg, str);
	strcat(msg, bcd);
	printCenter(row, msg);
}

// MAIN ////////////////////////////////////////////////////////////////////////

void main()
{
	unsigned char *pcart = (unsigned char *)0x8000;
	unsigned char  i; //, joybtns, bi;
	unsigned int   i16;
	unsigned int   ext_cart_id = 0xFFFF;
	char          *biosfile = NULL;
	fileTYPE       file;

	mach_id = MACHID;
	
	if (mach_id == 8)
		if ((STATUS & 0x01) == 0x01)
			startExtCart();

	vdp_init();
	vdp_setcolor(COLOR_BLACK, COLOR_BLACK, COLOR_WHITE);
	vdp_putstring(TITULO);

//	Joy combo BIOS selection
/*
	joybtns = ReadJoy();

	if ((joybtns & JOY_UP) != 0)
		bi = 1;
	else if ((joybtns & JOY_DOWN) != 0)
		bi = 2;
	else
		bi = 0;

	biosfile = (char *)biosfiles[bi];
	strcpy(msg, "LOADING ");
	strcat(msg, biosfile);
	printCenter(9, msg);
*/
/*
	byte = fiW1R1(FI_READ_STATUS_REGISTER_1);

	byte2bcd();
	strcpy(msg, "SR1: ");
	strcat(msg, bcd);
	printCenter(11, msg);

	byte = fiW1R1(FI_READ_STATUS_REGISTER_2);

	byte2bcd();
	strcpy(msg, "SR2: ");
	strcat(msg, bcd);
	printCenter(12, msg);

	byte = fiW1R1(FI_READ_STATUS_REGISTER_3);

	byte2bcd();
	strcpy(msg, "SR3: ");
	strcat(msg, bcd);
	printCenter(13, msg);
*/
//	SD card initialization & BIOS loading

	if (!MMC_Init()) erro("SD INI ERR");

	if (!FindDrive()) erro("NO SD PART");

	if (!FileOpen(&file, corefile))
//		erro("2: CORE FILE NOT FOUND");
		goto skipcore;

///	if (file.size != 475136)
//		erro("3: WRONG CORE SIZE");
///		goto skipcore;

	vdp_gotoxy(0, 2);

	for (i = 0; i < 8; i++)
	{
		vdp_putchar('E');
		fiWR(FI_WRITE_ENABLE    , 0, 0, 0, 0, 0, 0);
		fiWR(FI_64KB_BLOCK_ERASE, 3, 0, i, 0, 0, 0);
		fiWait();
	}

//	Read 16 blocks of 512 bytes (8192 bytes)

//	0000-039F
//	0000-073E

	vdp_gotoxy(0, 4);

	CONFIG = 3;

	for (i16 = 0; i16 < 928; i16++)
	{
		pbios = (unsigned char *) 0x8000;
		if (!FileRead(&file, pbios))
			erro("READ ERR");

		if ((i16 & 1) == 0) vdp_putchar('W');
		fiWR(FI_WRITE_ENABLE,   0, 0, 0, 0, 0, 0);
		fiWR(FI_PAGE_PROGRAM, 255, 0, (i16 << 1) >> 8, (i16 << 1) & 0xFF, 0, 0);
		fiWait();
		pbios = (unsigned char *) 0x8100;
		fiWR(FI_WRITE_ENABLE,   0, 0, 0, 0, 0, 0);
		fiWR(FI_PAGE_PROGRAM, 255, 0, (i16 << 1) >> 8, ((i16 << 1) & 0xFF) | 1, 0, 0);
		fiWait();

	}

//	fiWR(FI_WRITE_DISABLE, 0, 0, 0, 0, 0, 0);

	byte = fiWR(FI_READ_DATA, 3, 1, 0, 0, 0x11, 0);
	dump(22, "11:");
/*
	byte = pbios[0x11];
	dump(23, "BB:");
*/
//	byte = fiWR(FI_READ_DATA, 3, 1, 0, 0, 0, 0);
//	dump(16, "PP:");

	strcpy(msg, "OK");
	printCenter(20, msg);

freeze:

	goto freeze;

skipcore:

	if (!FileOpen(&file, biosfiles[0]))	erro("NO BIOS");

//	if (file.size != 8192) erro("BAD BIOS");

//	Read 16 blocks of 512 bytes (8192 bytes)
//	pbios = 0;
	for (i = 0; i < 16; i++)
	{
		if (!FileRead(&file, pbios))
			erro("ERR BIOS");
		pbios += 512;
	}

///	vdp_patchfont();

//	Test if external cartridge exists

	ext_cart_id = peek16(0x8000);

	if (ext_cart_id == 0x55AA || ext_cart_id == 0xAA55)

	//	Start external cartridge

		startExtCart();
	else
	{
	//	Load multicart ROM

		CONFIG = 0x03;
		strcpy(msg, "LD MULT");
		printCenter(10, msg);
	
		if (!FileOpen(&file, mcfile))
			erro("NO MULT");

		if (file.size != 16384)
			erro("BAD MULT");

	//	Read 32 blocks of 512 bytes (16384 bytes)

		for (i = 0; i < 32; i++)
		{
			if (!FileRead(&file, pcart))
				erro("ERR MULT");
			pcart += 512;
		}

		startMulticart();
	}
}
