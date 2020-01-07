// VDP header for the TI-99/4A by Tursi aka Mike Brent
// This code and library released into the Public Domain
// You can copy this file and use it at will ;)

//*********************
// VDP access ports
//*********************

// Read Data
volatile __sfr __at 0xbe VDPRD;
// Read Status
volatile __sfr __at 0xbf VDPST;
// Write Address/Register
volatile __sfr __at 0xbf VDPWA;
// Write Data
volatile __sfr __at 0xbe VDPWD;

//*********************
// Inline VDP helpers
//*********************

// safe delay between VDP accesses - each NOP is 4 on the Coleco (5 on MSX apparently! Watch out!)
// The VDP needs 29 cycles between accesses, roughly. The code generated often is slow enough, if variables are used instead
// of constants. If you need the speed, you could hand-optimize the asm knowing this. ;)
inline void VDP_SAFE_DELAY() {	
// still tuning this... from online comments:
// Actually, Z80 frequency is 3.579545 MHz (MSX) so the math comes to 28.63, roughly 29 T-states.
// You must include the OUT instruction, so it means we need 18 additional T-states before the next 
// VRAM access. That's a lot of unused CPU time, but if it is needed, so be it.
// When I did the math, I got this using OUTI:
// OUTI is 16 cycles or 4.47uS
// NOP is 4 cycles or 1.12uS
// to get to 8uS, we'd need 4 NOPS (although 3 would work 90% of the time... 0.12uS difference! So close! Maybe another instruction?)
// However, OUTI/OUTD are the slower instructions, the fastest OUT is OUT (p),A, which is 11 cycles (3.073 uS)
// To get to 8uS there, we do need 5 NOPs (although the last cycle is only off by 0.44uS)

__asm
	nop
	nop
	nop
	nop
	nop
__endasm;
}

// TODO: need some hardware testing to understand the VDP limits
// Can you write the address register full speed? Is it /only/ VRAM access that needs the delay?
// If true, these sequences are safe: 
//		ADR,ADR_WR,DATA,>DELAY<
//		ADR,ADR_RD,>DELAY<,DATA
// I've taken out the address write safety for that reason, we will see! Even the MSX guys aren't sure.
//#define PARANOID_TIMING

// Set VDP address for read (no bit added)
#ifdef PARANOID_TIMING
inline void VDP_SET_ADDRESS(unsigned int x)							{	VDPWA=((x)&0xff); VDP_SAFE_DELAY(); VDPWA=((x)>>8); __asm nop __endasm;	}
#else
inline void VDP_SET_ADDRESS(unsigned int x)							{	VDPWA=((x)&0xff); VDPWA=((x)>>8); VDP_SAFE_DELAY();	}
#endif

// Set VDP address for write (adds 0x4000 bit)
#ifdef PARANOID_TIMING
inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDP_SAFE_DELAY(); VDPWA=(((x)>>8)|0x40);	__asm nop __endasm; }
#else
inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDPWA=(((x)>>8)|0x40); }
#endif

// Set VDP write-only register 'r' to value 'v'
#ifdef PARANOID_TIMING
inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDP_SAFE_DELAY(); VDPWA=(0x80|(r));	__asm nop __endasm;		}
#else
inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
#endif

// get a screen offset for 32x24 graphics mode
inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}

// get a screen offset for 40x24 text mode
inline int VDP_SCREEN_TEXT(unsigned int r, unsigned char c)			{	return (((r)<<5)+((r)<<3)+(c));				}

//*********************
// VDP Console interrupt control
//*********************

// Interrupt counter - incremented each interrupt
extern volatile unsigned char VDP_INT_COUNTER;

// Maximum number of sprites performing automatic motion (not supported)
//#define VDP_SPRITE_MOTION_MAX	*((volatile unsigned char*)0x837a)

// Copy of the VDP status byte. If VDP interrupts are enabled, you should read
// this value, instead of reading it directly from the VDP.
extern volatile unsigned char VDP_STATUS_MIRROR;

// This flag byte allows you to turn parts of the console interrupt handler on and off
// See the VDP_INT_CTRL_* defines below (not needed today)
//extern unsigned char VDP_INT_CTRL;

// If using KSCAN, you must put a copy of VDP register 1 (returned by the 'set' functions)
// at this address, otherwise the first time a key is pressed, the value will be overwritten.
// The console uses this to undo the screen timeout blanking. (not needed)
//#define VDP_REG1_KSCAN_MIRROR	*((volatile unsigned char*)0x83d4)
//#define FIX_KSCAN(x) VDP_REG1_KSCAN_MIRROR=(x);
#define FIX_KSCAN(x)

// The console counts up the screen blank timeout here. You can reset it by writing 0,
// or prevent it from ever triggering by writing an odd number. Each interrupt, it is
// incremented by 2, and when the value reaches 0x0000, the screen will blank by setting
// the blanking bit in VDP register 1. This value is reset on keypress in KSCAN.
// Not supported
//#define VDP_SCREEN_TIMEOUT		*((volatile unsigned int*)0x83d6)

// These values are flags for the interrupt control 
	// disable all processing (screen timeout and user interrupt are still processed)
	#define VDP_INT_CTRL_DISABLE_ALL		0x80
	// disable sprite motion
	#define VDP_INT_CTRL_DISABLE_SPRITES	0x40
	// disable sound list processing
	#define VDP_INT_CTRL_DISABLE_SOUND		0x20
	// disable QUIT key testing
	#define VDP_INT_CTRL_DISABLE_QUIT		0x10

// wait for a vblank (limi interrupts disabled - will work unreliably if enabled)
// VDP hardware interrupt must be enabled!!
// there's no CRU on the Coleco, of course... but for compatibility..
extern volatile unsigned char vdpLimi;
#define VDP_WAIT_VBLANK_CRU	  while ((vdpLimi&0x80) == 0) { }

#define VDP_CLEAR_VBLANK { vdpLimi = 0; VDP_STATUS_MIRROR = VDPST; }

// we enable interrupts via a mask byte, as Coleco ints are NMI
// Note that the enable therefore needs to check a flag!
// Note that on the TI interrupts DISABLED is the default state
#define VDP_INT_ENABLE			{ __asm__("\tpush hl\n\tld hl,#_vdpLimi\n\tset 0,(hl)\n\tpop hl"); if (vdpLimi&0x80) my_nmi(); }
#define VDP_INT_DISABLE			{ __asm__("\tpush hl\n\tld hl,#_vdpLimi\n\tres 0,(hl)\n\tpop hl"); }
#define VDP_INT_POLL {	\
	VDP_INT_ENABLE;		\
	VDP_INT_DISABLE; }

//*********************
// Register settings
//*********************

// Bitmasks for the status register
#define VDP_ST_INT				0x80		// interrupt ready
#define VDP_ST_5SP				0x40		// 5 sprites-on-a-line
#define VDP_ST_COINC			0x20		// sprite coincidence
#define VDP_ST_MASK				0x1f		// mask for the 5 bits that indicate the fifth sprite on a line

// these are the actual write-only register indexes
#define VDP_REG_MODE0			0x00		// mode register 0
#define VDP_REG_MODE1			0x01		// mode register 1
#define VDP_REG_SIT				0x02		// screen image table address (this value times 0x0400)
#define VDP_REG_CT				0x03		// color table address (this value times 0x0040)
#define VDP_REG_PDT				0x04		// pattern descriptor table address (this value times 0x0800)
#define VDP_REG_SAL				0x05		// sprite attribute list address (this value times 0x0080)
#define VDP_REG_SDT				0x06		// sprite descriptor table address (this value times 0x0800)
#define VDP_REG_COL				0x07		// screen color (most significant nibble - foreground in text, least significant nibble - background in all modes)

// settings for mode register 0
#define VDP_MODE0_BITMAP		0x02		// set bitmap mode
#define VDP_MODE0_EXTVID		0x01		// enable external video (not connected on TI-99/4A)

// settings for mode register 1
#define VDP_MODE1_16K			0x80		// set 16k mode (4k mode if cleared)
#define VDP_MODE1_UNBLANK		0x40		// set to enable display, clear to blank it
#define VDP_MODE1_INT			0x20		// enable VDP interrupts
#define VDP_MODE1_TEXT			0x10		// set text mode
#define VDP_MODE1_MULTI			0x08		// set multicolor mode
#define VDP_MODE1_SPRMODE16x16	0x02		// set 16x16 sprites
#define VDP_MODE1_SPRMAG		0x01		// set magnified sprites (2x2 pixels) 

// sprite modes for the mode set functions
#define VDP_SPR_8x8				0x00
#define	VDP_SPR_8x8MAG			(VDP_MODE1_SPRMAG)
#define VDP_SPR_16x16			(VDP_MODE1_SPRMODE16x16)
#define VDP_SPR_16x16MAG		(VDP_MODE1_SPRMODE16x16 | VDP_MODE1_SPRMAG)

// VDP colors
#define COLOR_TRANS				0x00
#define COLOR_BLACK				0x01
#define COLOR_MEDGREEN			0x02
#define COLOR_LTGREEN			0x03
#define COLOR_DKBLUE			0x04
#define COLOR_LTBLUE			0x05
#define COLOR_DKRED				0x06
#define COLOR_CYAN				0x07
#define COLOR_MEDRED			0x08
#define COLOR_LTRED				0x09
#define COLOR_DKYELLOW			0x0A
#define COLOR_LTYELLOW			0x0B
#define COLOR_DKGREEN			0x0C
#define COLOR_MAGENTA			0x0D
#define COLOR_GRAY				0x0E
#define COLOR_WHITE				0x0F

//*********************
// VDP related functions
//*********************

// set_graphics - sets up graphics I mode - 32x24, 256 chars, color, sprites
// Inputs: pass in VDP_SPR_xxx for the sprite mode you want
// Return: returns a value to be written to VDP_REG_MODE1 (and VDP_REG1_KSCAN_MIRROR if you use kscan())
// The screen is blanked until you do this write, to allow you time to set it up
unsigned char set_graphics(unsigned char sprite_mode);

// set_text - sets up text mode - 40x24, 256 chars, monochrome (color set by VDP_REG_COL), no sprites
// Inputs: none
// Return: returns a value to be written to VDP_REG_MODE1 (and VDP_REG1_KSCAN_MIRROR if you use kscan())
// The screen is blanked until you do this write, to allow you time to set it up
unsigned char set_text();

// set_multicolor - sets up multicolor mode - 64x48, 256 chars, color, sprites
// Inputs: pass in VDP_SPR_xxx for the sprite mode you want
// Return: returns a value to be written to VDP_REG_MODE1 (and VDP_REG1_KSCAN_MIRROR if you use kscan())
// The screen is blanked until you do this write, to allow you time to set it up
unsigned char set_multicolor(unsigned char sprite_mode);

// set_bitmap - sets up graphics II (aka bitmap) mode - 32x24, 768 chars in three zones, color, sprites
// Inputs: pass in VDP_SPR_xxx for the sprite mode you want
// Return: returns a value to be written to VDP_REG_MODE1 (and VDP_REG1_KSCAN_MIRROR if you use kscan())
// The screen is blanked until you do this write, to allow you time to set it up
unsigned char set_bitmap(unsigned char sprite_mode);

// writestring - writes an arbitrary string of characters at any position on the screen
// Inputs: row and column (zero-based), NUL-terminated string to write
// Note: will not write the correct location in text mode
void writestring(unsigned char row, unsigned char col, char *pStr);

// vdpmemset - sets a count of VDP memory bytes to a value
// Inputs: VDP address to start, the byte to set, and number of repeats
void vdpmemset(int pAddr, unsigned char ch, int cnt);

// vdpmemcpy - copies a block of data from CPU to VDP memory
// Inputs: VDP address to write to, CPU address to copy from, number of bytes to copy
void vdpmemcpy(int pAddr, const unsigned char *pSrc, int cnt);

// vdpmemread - copies a block of data from VDP to CPU memory
// Inputs: VDP address to read from, CPU address to write to, number of bytes to copy
void vdpmemread(int pAddr, unsigned char *pDest, int cnt);

// vdpwriteinc - writes an incrementing sequence of values to VDP
// Inputs: VDP address to start, first value to write, number of bytes to write
// This is intended to be useful for setting up bitmap and multicolor mode with
// incrementing tables
void vdpwriteinc(int pAddr, unsigned char nStart, int cnt);

// vdpchar - write a character to VDP memory (NOT to be confused with basic's CALL CHAR)
// Inputs: VDP address to write, character to be written
void vdpchar(int pAddr, unsigned char ch);

// vdpreadchar - read a character from VDP memory
// Inputs: VDP address to read
// Outputs: byte
unsigned char vdpreadchar(int pAddr);

// vdpwritescreeninc - like vdpwriteinc, but writes to the screen image table
// Inputs: offset from the screen image table to write, first value to write, number of bytes to write
void vdpwritescreeninc(int pAddr, unsigned char nStart, int cnt);

// vdpscreenchar - like vdpchar, but writes to the screen image table
// Inputs: offset from the screen image table to write to, value to be written
void vdpscreenchar(int pAddr, unsigned char ch);

// vdpwaitvint - enables console interrupts, then waits for one to happen
// Interrupts are disabled upon exit.
// returns non-zero if the interrupt fired before entry (ie: we are late)
unsigned char vdpwaitvint();

// vdpputchar - writes a single character with limited formatting to the bottom of the screen
// Inputs: character to emit
// Returns: character input
// All characters are emitted except \r and \n which is handled for scrn_scroll and next line.
// It works in both 32x24 and 40x24 modes. Tracking of the cursor is thus 
// automatic in this function, and it pulls in scrn_scroll.
int vdpputchar(int x);

// putstring - writes a string with limited formatting to the bottom of the screen
// Inputs: NUL-terminated string to write
// This function only emits printable ASCII characters (32-127). It works in both
// 32x24 and 40x24 modes. It recognizes \r to go to the beginning of the line, and
// \n to go to a new line and scroll the screen. Tracking of the cursor is thus 
// automatic in this function, and it pulls in scrn_scroll.
void putstring(char *s);

// vdpprintf - writes a string with limited formatting. Only supports a very small subset
// of formatting at the moment. Supports width (for most fields), s, u, i, d, c and X
// (X is byte only). This function will call in putchar().
// Inputs: format string, and varable argument list
// Returns: always returns 0
int vdpprintf(char *str, ...);

// raw_vdpmemset - sets bytes at the current VDP address
void raw_vdpmemset(unsigned char ch, int cnt);

// raw_vdpmemcpy - copies bytes from CPU to current VDP address
void raw_vdpmemcpy(unsigned char *p, int cnt);

// putstring - writes a string with limited formatting to the bottom of the screen
// Inputs: NUL-terminated string to write
// This function only emits printable ASCII characters (32-127). It works in both
// 32x24 and 40x24 modes. It recognizes \r to go to the beginning of the line, and
// \n to go to a new line and scroll the screen. Tracking of the cursor is thus 
// automatic in this function, and it pulls in scrn_scroll.
void putstring(char *s);

// hexprint - generates a 2 character hex string from an int and calls putstring to print it
void hexprint(unsigned char x);

// fast_hexprint - generates a 2 character hex string from an int and calls putstring to print it
// uses a 512 byte lookup table - so it is fast but costs more to use
void fast_hexprint(unsigned char x);

// faster_hexprint - works like fast_hexprint but displays directly to VDPWD, no formatting or
// scroll and you must set the VDP address before calling
void faster_hexprint(unsigned char x);

// scrn_scroll - scrolls the screen upwards one line - works in 32x24 and 40x24 modes
void scrn_scroll();

// hchar - repeat a character horizontally on the screen, similar to CALL HCHAR
// Inputs: row and column (0-based, not 1-based) to start, character to repeat, number of repetitions (not optional)
// Note: for a single character, vdpscreenchar() is more efficient
void hchar(unsigned char r, unsigned char c, unsigned char ch, int cnt);

// vchar - repeat a character vertically on the screen, similar to CALL VCHAR
// Inputs: row and column (0-based, not 1-based), character to repeat, number of repetitions (not optional)
// Note: for a single character, vdpscreenchar() is more efficient
void vchar(unsigned char r, unsigned char c, unsigned char ch, int cnt);

// gchar - return a character from the screen, similar to CALL GCHAR
// Inputs: row and column (0-based, not 1-based) to read from
// Return: character at the specified position on the screen
unsigned char gchar(unsigned char r, unsigned char c);

// sprite - set up an entry in the sprite attribute list, similar to CALL SPRITE
// Inputs: sprite number (0-31), character (0-255), color (COLOR_xx), row and column (0-based)
// Note that motion set up is not handled by this function
// Note that row 255 is the first line on the screen
// And finally, note that a row of 208 will disable display of all subsequent sprite numbers
void sprite(unsigned char n, unsigned char ch, unsigned char col, unsigned char r, unsigned char c);

// delsprite - remove a sprite by placing it offscreen
// Inputs: sprite number (0-31) to hide
void delsprite(unsigned char n);

// charset - load the default character set from GROM. This will load both upper and lowercase (small capital) characters.
// Not compatible with the 99/4, if it matters.
void charset();

// charsetlc - load the default character set including true lowercase. This code includes a lower-case character
// set and shifts the GROM character set to align better with it. Because it pulls in data, it does take a little more
// memory (208 bytes). Not compatible with the 99/4, if it matters.
void charsetlc();

// gplvdp - copy data from a GPL function to VDP memory. 
// Inputs: address of a GPL vector, VDP address to copy to, number of characters to copy
// This is a very specialized function used by the charset() functions. It assumes a GPL 'B'
// instruction at the vector, and that the first instruction thereafter is a 'DEST'. It uses
// this to find the actual character set data regardless of the GROM version. This function
// is not compatible with the 99/4 (because it copies 7 bytes per character, and the 99/4
// only provided 6 bytes). (not supported)
//void gplvdp(int vect, int adr, int cnt);

// user interrupt access helpers (for more portable code)
//void vdpinit();	called automatically, don't use
void setUserIntHook(void (*hookfn)());
void clearUserIntHook();
void my_nmi();

// global pointers for all to enjoy - make sure the screen setup code updates them!
// assumptions here are for E/A environment, they may not be accurate and your
// program should NOT trust them until after one of the mode set functions is called.
extern unsigned int gImage;					// SIT, Register 2 * 0x400
extern unsigned int gColor;					// CR,  Register 3 * 0x40
extern unsigned int gPattern;				// PDT, Register 4 * 0x800
extern unsigned int gSprite;				// SAL, Register 5 * 0x80
extern unsigned int gSpritePat;				// SDT, Register 6 * 0x800

// text position information used by putstring and scrn_scroll
extern int nTextRow,nTextEnd;
extern int nTextPos;

extern unsigned char gSaveIntCnt;	// console interrupt count byte

// 512 byte lookup table for converting a byte to two ASCII hex characters
extern const unsigned int byte2hex[256];
