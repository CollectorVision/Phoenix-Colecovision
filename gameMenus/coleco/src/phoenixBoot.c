//******************************
// New Phoenix boot BIOS
// Created by Tursi/M.Brent
// Boot logo and SD menu
// Uses libti99Coleco - https://github.com/tursilion/libti99coleco - cef5a4dbef4bb873b088ca290c3592be8ebb69c4
// Note: we have 24k now per the new memory layout, and 8k RAM at 0x6000 (First 1k is true Coleco RAM)

#include "memset.h"
#include "vdp.h"
#include "phoenix.h"
#include "f18a.h"
#include "joykey.h"

#define HEADERS__
#include "newmap2.c"
#include "mattfont.c"
#undef HEADERS__

// - stored in the F18A code - actual value of the 0x32 register (so it changes in text mode)
extern unsigned char useScanlines;     // takes in real time but we need it after a reset
unsigned char useFlicker=0;            // can't enable this until we start a cartridge
unsigned char text_width=32;           // needed for the menu popup
extern void inc_palette();

// --- externs ---

// fast memcpy, since we don't need delays on the F18A
extern void vdpmemcpyfast(unsigned int pAddr, const unsigned char *pSrc, unsigned int cnt);
#define vdpmemcpy vdpmemcpyfast
// menu function
extern void menu();

// --- hardware ---
static volatile __sfr __at 0xc0 port3;  // joystick reset strobe

// macro for accessing ROM inline - this optimizes as you'd expect
#define pRom(x) *((volatile unsigned char*)(x))

// --- definitions ---

// cartridge name format: "GAME NAME/COMPANY/1982"
// There is no explicit limit on the lengths of the first two strings
#define CART_NAME_INFO 0x8024

// --- work buffers ---

// cartridge string (we format it into here)
char copyright[65];
// screen line buffer - shoving this larger array out of the way
// down in the Coleco RAM space - remember not to access it after
// beginning the cartridge start sequence!
unsigned char lineBuffer[40*16];

// --- static data ---

// Coleco color table at 0x2003
static const unsigned char colorTable[] = {
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
    0xf0, 0xd0, 0x80, 0x90, 0xb0, 0x30, 0x40
};

// Coleco VDP register table at 0x63f2
//pRom(0x63f2+0*2)   = 0x00;
//pRom(0x63f2+0*2+1) = 0x1B;
//pRom(0x63f2+1*2)   = 0x00;
//pRom(0x63f2+1*2+1) = 0x38;
//pRom(0x63f2+2*2)   = 0x00;
//pRom(0x63f2+2*2+1) = 0x18;
//pRom(0x63f2+3*2)   = 0x00;
//pRom(0x63f2+3*2+1) = 0x00;
//pRom(0x63f2+4*2)   = 0x00;
//pRom(0x63f2+4*2+1) = 0x20;
static const unsigned char regTable[] = {
    0x00, 0x1b, 0x00, 0x38, 0x00, 
    0x18, 0x00, 0x00, 0x00, 0x20
};

// trampoline to start the cart with the Coleco BIOS loaded
// This area (0x6000) seems untouched by the Coleco BIOS
static const unsigned char trampoline[] = {
    0xdb, 0x55,         // -        IN 0x55,A           DB55    swaps the BIOS - note RAM goes from 8k to 1k!
    0x31, 0xb9, 0x73,   // -        LD SP,0x73B9        31B973
    0x2a, 0x0a, 0x80,   // -        LD HL,[0x800A]      2A800A

//  0xC3,0x00,0x00,      // JP 0x0000 (reboot bios)

    0xe9                // -        JP [HL]             E9
};

// --- and functions... ---
void readkeypad();

// setting up standard screen modes - blanks the screen and returns the unblank command
// interrupts are also disabled. Unblank will re-enable them, too, write it to VDP_REG_MODE1
// this is a stripped down version out of libti99Coleco that doesn't set the sprite table or tracking variables
// A few registers also differ. text mode in menu.c shares this layout.
unsigned char my_set_graphics(unsigned char sprite_mode) {
	// this layout is untested but should match editor/assembler's
	VDP_SET_REGISTER(VDP_REG_MODE1, VDP_MODE1_16K);		// no need to OR in the sprite mode for now
	VDP_SET_REGISTER(VDP_REG_MODE0, 0);
	VDP_SET_REGISTER(VDP_REG_SIT, 0x00);    // 0x0000
	VDP_SET_REGISTER(VDP_REG_CT, 64);       // 0x1000
	VDP_SET_REGISTER(VDP_REG_PDT, 0x01);    // 0x0800
	VDP_SET_REGISTER(VDP_REG_SAL, 14);      // 0x0700
	VDP_SET_REGISTER(VDP_REG_SDT, 0x03);    // 0x1800
	VDP_SET_REGISTER(VDP_REG_COL, ((COLOR_GRAY<<4)|COLOR_BLACK));
    text_width = 32;
	return VDP_MODE1_16K | VDP_MODE1_UNBLANK | VDP_MODE1_INT | sprite_mode;
}

// unRLE CPU to VDP handler
// Basic RLE compression applied to PAT0
// top two msbits are flags:
// 00 - run of zeros
// 01 - run of 0xff
// 10 - run of next char
// 11 - inline string
// least significant 6 bits are the length
// Warning: No sanity checking takes place
void unrle(unsigned int vdpAdr, unsigned char *pSrc, unsigned int rleCnt) {
    unsigned char *pEnd;
    
    VDP_SET_ADDRESS_WRITE(vdpAdr);
    pEnd = pSrc + rleCnt;

    while (pSrc < pEnd) {
        unsigned char cnt = *(pSrc++);
        unsigned char byte = 0;
        switch (cnt&0xc0) {
            case 0x00:
                // run of zeros
                byte = 0;
                break;

            case 0x40:
                // run of 0xffs
                byte = 0xff;
                break;

            case 0x80:
                // run of next byte
                byte = *(pSrc++);
                break;

            case 0xc0:
                // inline string (special case)
                cnt = (cnt & 0x3f) + 1;
                while (cnt--) {
                    VDPWD = *(pSrc++);
                }
                continue;
        }
        // all other cases come down here to be a run of bytes
        cnt = (cnt & 0x3f) + 1;
        while (cnt--) {
            VDPWD = byte;
        }
    }
}

// delay for 'x' vblanks - zero will just clear any pending
// beware of calling this in a loop - if the time between calls
// is longer than a frame you will clear the pending vblank on
// entry and wait for another one (ie: you may wait a frame
// longer than you expect).
void waitVblanks(unsigned char cnt) {
    // clear any vblank we may have missed
    VDP_CLEAR_VBLANK;

    while (cnt-- > 0) {
    	// wait as instructed
    	VDP_WAIT_VBLANK_CRU;
    	VDP_CLEAR_VBLANK;
    }
}

// simple config menu for scanlines and flicker that we can activate
// even before a cartridge boots - called ONLY from readkeypad()
void cfgMenu() {
    unsigned char lastkey = 0xff;
    unsigned int pos = GIMAGE+8*40;
    unsigned int onoff;
    unsigned char de1 = 0, de2 = 0;

    // if we're in gfx mode, then the logo is up, which means we don't
    // have a character set. Rather than rewrite everything for the bitmap
    // overlay right now, we'll just fake it and load the chars we need.
    // With a little luck, they'll be under the window... ;)
    // FATFONT starts at char 29, and I want starting at char 48...
	if (text_width == 32) {
		vdpmemread(GIMAGE+8*32, lineBuffer, 16*32);		// backup screen
		vdpmemset(GIMAGE+8*32, 0x18, 16*32);			// clear it
        vdpmemcpy(GPATTERN+' '*8, &FATFONT[(' '-29)*8], ('X'-' '+1)*8);		// fix char set
        pos = GIMAGE+8*32;
    } else {
		vdpmemread(GIMAGE+8*40, lineBuffer, 8*40);		// backup screen
		vdpmemset(GIMAGE+8*40, ' ', 8*40);				// clear it
	}

    // We might be in gfx or text mode, so we'll just left-align, sort of
    pos += text_width+3;
    vdpmemcpy(pos, "PRESS:", 6);
    pos += text_width+2;
    vdpmemcpy(pos, "1 TOGGLE SCANLINES", 18);
    pos += text_width;
    vdpmemcpy(pos, "2 TOGGLE FLICKER (   )", 22);
    onoff = pos+18;
    pos += text_width + text_width;
    vdpmemcpy(pos, "9 EXIT MENU", 11);

    for (;;) {
		waitVblanks(1);
		if (useFlicker) {
			vdpmemcpy(onoff, "ON ", 3);
		} else {
			vdpmemcpy(onoff, "OFF", 3);
		}
		if (de1) --de1;
		if (de2) --de2;

        readkeypad();
        if (MY_KEY != lastkey) {
            lastkey = MY_KEY;

            if (lastkey == '9') {
                break;
            } else if (lastkey == '1') {
				if (de1 == 0) {
					// toggle scanlines - this is pretty obvious so no feedback needed
					useScanlines ^= SCANLINES_ON;
					VDP_SET_REGISTER(0x32, useScanlines);
					de1 = 15;
				}
            } else if (lastkey == '2') {
				if (de2 == 0) {
					// toggle flicker and update display
					useFlicker = !useFlicker;
					de2 = 15;
				}
            }
        }
    }
 
    // restore the display

    // in graphics mode, restore the font - we have to do it all since
    // we can't (trivally) unrle only part of it.
    if (text_width == 32) {
		vdpmemset(GIMAGE+8*32, 0x18, 16*32);		// clear it
        unrle(GPATTERN, PAT0, SIZE_OF_PAT0RLE);		// reload character set
		vdpmemcpy(GIMAGE+8*32, lineBuffer, 16*32);	// restore display
    } else {
		vdpmemcpy(GIMAGE+8*40, lineBuffer, 8*40);	// restore display
    }
}

// wrapper for kscan that knows about our hot config keys and reads both sticks
// this allows them to work at pretty much any time automatically
void readkeypad() {
    static unsigned char lastret = 0xff;
    static unsigned char inCfg = 0;
    
    readinputs(1);
    if ((MY_KEY == 0xff)&&(MY_JOYX==0)&&(MY_JOYY==0)) {
        // try joystick 2
        readinputs(2);
    }

    // check only when changed
    if ((!inCfg) && (lastret != MY_KEY)) {
        lastret = MY_KEY;
        if ((MY_KEY == '*')||(MY_KEY == '#')) {
            inCfg = 1;
            cfgMenu();
            inCfg = 0;
        } else if (MY_KEY == '0') {
			// toggle palette
			inc_palette();
		}
    }
}

// text out to the sprite layer at the bottom of the screen
// VDP is the starting address in the sprite pattern table
// Sprites are stacked 2 vertical before each column, and 8 bytes per char
void textout(unsigned int vdp, const char *p) {
	while (*p) {
		unsigned char c = (*(p++))-' '+3;
        unsigned int off = c*8;
        vdpmemcpy(vdp, &FATFONT[off], 8);
        vdp+=16;
	}
}

// same as textout, but shifts the text half a character to the right
// this would have been much easier with the bitmap layer...
void textouthalf(unsigned int vdp, const char *p) {
    unsigned char first = 1;
	while (*p) {
        unsigned char i;
        VDP_SET_ADDRESS_WRITE(vdp);
        for (i=0; i<8; ++i) {
            unsigned char c1,c2,out;
            unsigned int off1,off2;

            if (first) {
                c1 = 3;     // space is 3
                c2 = (*p)-' '+3;
            } else {
                c1 = (*p)-' '+3;
                if (*(p+1) == 0) {
                    c2 = 3;
                } else {
                    c2 = (*(p+1))-' '+3;
                }
            }
            off1 = c1*8+i;
            off2 = c2*8+i;

            out = (FATFONT[off1]<<4) | (FATFONT[off2]>>4);
            VDPWD = out;
        }
        vdp+=16;
        if (first) {
            first = 0;
        } else {
            ++p;
        }
	}
}

// fade in the PHOENIX text using F18A palette
void fadeInTextF18A() {
	unsigned char p = 0;
	unsigned char i;

	// first set the color for white to black, we'll fade it in
	VDP_SET_REGISTER(0x2f, 0x8f);	// Reg 47, value: 1000 1111, DPM = 1, AUTO INC = 0, palreg 15.
	VDPWD = 0;
	VDPWD = 0;						// color is now black
	//VDP_SET_REGISTER(0x2f, 0x00);	// Turn off the DPM mode

	// now set the phoenix color sets (3-12) to white on transparent
	vdpmemset(GCOLOR+3, 0xf0, 10);
	
	// now copy the characters into place
	vdpmemcpy(GIMAGE+(18*32), MD0_PHOENIX, 4*32);

	// now we can fade it in with normal interrupt polling
	for (i=1; i<16; ++i) {
	    // turn DPM back on (auto-off after one reg when there's no auto-inc)
	    VDP_SET_REGISTER(0x2f, 0x8f);	// Reg 47, value: 1000 1111, DPM = 1, AUTO INC = 0, palreg 15.

        // set the color register
		VDPWD = i;	// red
		VDPWD = i | (i<<4);	// green and blue
		
        waitVblanks(3);
	}
}

// animate the Phoenix logo up
void displayLogo() {
    unsigned char idx;
	unsigned int off;
    unsigned char const *p;
	
	// set the logo color to dark red - logo color sets are 13-31
	vdpmemset(GCOLOR+13, COLOR_DKRED<<4, 19);

	// the logo graphics occupy from roms 1 to 16
    // they are sorted from the bottom up, and use
    // a very simple RLE format (more of an alignment tag)
    // <#spaces>,<cnt>,<literal bytes>
	// we'll just draw from bottom up
    p = MD0_LOGO;
	for (idx = 16; idx > 0; --idx) {
        unsigned char len;
        waitVblanks(2);
		off = (idx-1)*32+(*(p++));
        len = *(p++);
		vdpmemcpy(GIMAGE+off, p, len);
        p+=len;
	}
}

// center a string on the sprite output lines
// takes half-characters into account and calls 
// the appropriate function. String must be less
// than 256 characters. (Better be less than 32)
void centerString(unsigned char line, char *p) {
    unsigned int vdp = GSPRITEPAT + (line*8);
    unsigned char len = 0;
    char *pp = p;

    while (*pp) {
        ++pp;
        if (pp - p > 31) {
            *pp='\0';
            break;
        }
    }
    len = pp-p;
    vdp += (16-(len/2))*16;

    if (len&1) {
        // odd offset
        textouthalf(vdp-16, p);
    } else {
        // even offset
        textout(vdp, p);
    }
}

// start whatever is configured at 0x8000
// This function is responsible for preparing Coleco RAM and
// VDP RAM to resemble what a real Coleco BIOS would have done,
// then to switch that BIOS in and start the title (without running
// THROUGH the BIOS).
void startTitle() {
    unsigned int idx;

    // - execute title
    // -    disable screen and interrupts
    // VDP_SET_REGISTER(VDP_REG_MODE1, VDP_MODE1_16K); - done by prepare_f18a()

    // -    reset F18A
    useScanlines &= SCANLINES_ON;  // turn off every bit except scanline selection
    prepare_f18a();   // also locks

    // the RAM located at 0x6000 is the original ColecoVision 1k
    // but unlike the Coleco BIOS, we /must/ access at 0x6000, not
    // one of the later mirrors.

    // Clear ram with an alternating pattern to simulate dram init
    // A real Coleco is far more random, but this should do for now
    for (idx=0x6000; idx<0x6400; idx+=2) {
		*((volatile unsigned int*)(idx)) = 0x00ff;
    }

    // -    prepare system as per original BIOS:
    // -        In test mode, it doesn't do ANYTHING, it just jumps to the cart immediately (must honor to avoid pointers)
    // We don't have to check both bytes because we already did that
    if (pRom(0x8000) == 0xaa) {
        // do the system init

        // -        Otherwise, it turns off the sound chip (already done)
        // -        Init the random number generator
        // -            LD HL, 0x33
        // -            LD [RAND_NUM], HL  (0x73c8)
        pRom(0x63c8) = 0x33;

        // -        Init the controller BIOS
        // -            CALL CONTROLLER_INIT (0x1105)
        // -                OUT [STRB_RST_PORT],A  (0xc0, A last contained 0xff)
        port3 = 0xff;

        // This block takes a user-specified RAM address from 0x8008,
        // and starting 2 bytes after that point, it zeroes 10 bytes
        // It also zeros 20 bytes starting at 0x7307
        // -                XOR A             (A = 0)
        // -                LD IX,[CONTROLLER_MAP] (0x8008)
        // -                INC IX
        // -                INC IX
        // -                LD IY,DBNCE_BUFF  (0x7307)
        // -                LD B,NUM_DEV*2    (NUM_DEV=5, value=10)
        // -              CINIT1
        // -                LD [IX+0],A
        // -                INC IX
        // -                LD [IY+0],A
        // -                INC IY
        // -                LD [IY+0],A
        // -                INC IY
        // -                DEC B
        // -                JR NZ,CINIT1
        {
            unsigned char *pDst = (unsigned char*)(*((volatile unsigned int*)0x8008)) + 2;
            memset(pDst, 0, 10);
            memset((void*)0x7307, 0, 20);
        }

        // This part just zeros a few fixed addresses
        // -                LD [SPIN_SW0_CT],A  (0x73eb)
        // -                LD [SPIN_SW1,CT],A  (0x73ec)
        // -                LD [S0_C0],A    (0x73ee)
        // -                LD [S0_C1],A    (0x73ef)
        // -                LD [S1_C0],A    (0x73f0)
        // -                LD [S1_C1],A    (0x73f1)
        // -                RET
        memset((void*)0x73eb, 0, 6);

        // -        Clear defer writes
        // -            LD A,FALSE
        // -            LD [DEFER_WRITES],A  (0x73c6)
        pRom(0x63c6) = 0;

        // -        Disable sprite mux
        // -            LD [MUX_SPRITES],A  (0x73c7)
        pRom(0x63c7) = 0;

        // -        Clear Video RAM
        // -            LD HL,0
        // -            LD DE,16384
        // -            LD A,0
        // -            CALL FILL_VRAM  (0x1804)
        // -                LD C,A
        // -                LD A,l
        // -                OUT [MODE_1_PORT],A
        // -                LD A,H
        // -                OR 0x40
        // -                OUT [MODE_1_PORT],A
        // -                LD A,C
        // -                OUT [MODE_0_PORT],A
        // -                DEC DE
        // -                LD A,D
        // -                OR E
        // -                JR NZ,FILL
        vdpmemset(0, 0, 16384);

        // -                CALL READ_REGISTER (0x1fdc)
        // -                    IN A,[CTRL_PORT] (0xbf) // Read VDP Status
        // -                    RET
        // -                RET
        {
            volatile unsigned char x = VDPST;
        }

        // -        Set VDP to mode 1 (display off)
        // -            CALL MODE_1 (0x18E9)
        // -                LD B,0      // reg
        // -                LD C,0      // value
        // -                CALL WRITE_REGISTER (0x1fd9->1CCA)
        // -                    LD A,C
        // -                    OUT [CTRL_PORT],A       // value
        // -                    LD A,B
        // -                    ADD A,0x80
        // -                    OUT [CTRL_PORT],A       // register number
        // -                    LD A,B
        // -                    CP 0
        // -                    JR NZ,NOT_REG_0
        // -                    LD A,C
        // -                    LD [VDP_MODE_WORD],A    // copy reg 0 to 0x73c3
        // -                  NOT_REG_0:
        // -                    LD A,B
        // -                    CP 1
        // -                    JR NZ,NOT_REG_1
        // -                    LD A,C
        // -                    LD [VDP_MODE_WORD+1],A  // copy reg 1 to 0x73c4
        // -                  NOT_REG_1:
        // -                    RET
        VDP_SET_REGISTER(0, 0);
        pRom(0x63c3) = 0;

        // -                LD B,1
        // -                LD C,0x80
        // -                CALL WRITE_REGISTER (0x1fd9->1CCA)
        VDP_SET_REGISTER(1, 0x80);
        pRom(0x63c4) = 0x80;

        // -                LD A,2          // (SIT) table code (0=sprite name, 1=sprite gen, 2=pattern name, 3=pattern gen, 4=color)
        // -                LD HL,0x1800    // VRAM address
        // -                CALL INIT_TABLE  (0x1fb8->1B1D) (this function is horrible!)
        // -                    LD C,A
        // -                    LD B,0
        // -                    LD IX,VRAM_ADDR_TABLE (0x73F2)
        // -                    ADD IX,BC
        // -                    ADD IX,BC       // add index*2
        // -                    LD [IX+0],L     // save VRAM address in table at 0x73F2
        // -                    LD [IX+1],H     // This is the part that really matters (besides setting the register)
        // -                    LD A,[VDP_MODE_WORD] (0x73c3 as set above)
        // -                    BIT 1,A
        // -                    JR Z,INIT_TABLE80
        // -                    LD A,C              // this is a big special case block for bitmap mode
        // -                    CP 3                // check generator table
        // -                    JR Z,CASE_OF_GEN    
        // -                    CP 4
        // -                    JR Z,CASE_OF_COLOR
        // -                    JR INIT_TABLE80
        // -                  CASE_OF_GEN:
        // -                    LD B,4              // VDP reg 4
        // -                    LD A,L              // check which 8k boundary
        // -                    OR H
        // -                    JR NZ,CASE_OF_GEN10
        // -                    LD C,3              // value to write for 0x0000
        // -                    JR INIT_TABLE90
        // -                  CASE_OF_GEN10:
        // -                    LD C,7              // value to write for 0x2000
        // -                    JR INIT_TABLE90
        // -                  CASE_OF_COLOR:
        // -                    LD B,3              // VDP reg 3
        // -                    LD A,L
        // -                    OR H
        // -                    JR NZ,CASE_OF_CLR10
        // -                    LD C,0x7f           // value for 0x0000
        // -                    JR INIT_TABLE90
        // -                  CASE_OF_CLR10
        // -                    LD C,0xff           // value for 0x2000
        // -                    JR INIT_TABLE90
        // -                  INIT_TABLE80
        // -                    LD IY,BASE_FACTORS  // this is the normal math code for non-bitmap
        // -                    ADD IY,BC
        // -                    ADD IY,BC
        // -                    LD A,[IY+0]         // get shift count from table
        // -                    LD B,[IY+1]         // get register number from table
        // -                  DIVIDE:
        // -                    SRL H
        // -                    RR L
        // -                    DEC A
        // -                    JR NZ,DIVIDE
        // -                    LD C,L              // get value for register into C
        // -                  INIT_TABLE90:
        // -                    CALL REG_WRITE      // write the register
        // -                    RET
        // -                  BASE_FACTORS:
        // -                    DEFB 7,5,11,6,10,2,11,4,6,3
        // (So, SIT = 0x1800, and store at 73F2+2*2
        // the memory writes are deferred to a single memcpy below
        VDP_SET_REGISTER(VDP_REG_SIT, 0x1800/0x400);
//        pRom(0x63f2+2*2) = 0x00;
//        pRom(0x63f2+2*2+1) = 0x18;

        // -                LD A,4          // color table
        // -                LD HL,0x2000
        // -                CALL INIT_TABLE  (0x1fb8)
        // (So, CT = 0x2000, and store at 73F2+4*2
        VDP_SET_REGISTER(VDP_REG_CT, 0x2000/0x40);
//        pRom(0x63f2+4*2) = 0x00;
//        pRom(0x63f2+4*2+1) = 0x20;

        // -                LD A,3          // pattern generator (PDT)
        // -                LD HL,0
        // -                CALL INIT_TABLE  (0x1fb8)
        // (So, PDT = 0x0000, and store at 73F2+3*2
        VDP_SET_REGISTER(VDP_REG_PDT, 0x0000/0x800);
//        pRom(0x63f2+3*2) = 0x00;
//        pRom(0x63f2+3*2+1) = 0x00;

        // -                LD A,0          // sprite name table (SAL)
        // -                LD HL,0x1B00
        // -                CALL INIT_TABLE  (0x1fb8)
        // (So, SAL = 0x1B00, and store at 73F2+0*2
        VDP_SET_REGISTER(VDP_REG_SAL, 0x1B00/0x80);
//        pRom(0x63f2+0*2) = 0x00;
//        pRom(0x63f2+0*2+1) = 0x1B;

        // -                LD A,1          // sprite generator (SDT)
        // -                LD HL,0x3800
        // -                CALL INIT_TABLE  (0x1fb8)
        // (So, SDT = 0x3800, and store at 73F2+1*2
        VDP_SET_REGISTER(VDP_REG_SDT, 0x3800/0x800);
//        pRom(0x63f2+1*2) = 0x00;
//        pRom(0x63f2+1*2+1) = 0x38;

        // doing the deferred table update all at once here
        memcpy((unsigned char*)0x63f2, regTable, 10);

        // -                LD B,7
        // -                LD C,0
        // -                CALL WRITE_REGISTER (0x1fd9)    // black screen
        // -                RET
        //VDP_SET_REGISTER(7, 0); // done by prepare_f18a

        // -        Load character set
        // -            CALL LOAD_ASCII (0x1927)
        // -                LD HL,ASC_TABLE (0x158b)    // 0x158B starts with 0x1D (copyright symbol)
        // -                LD DE,0x1d                  // first character index
        // -                LD IY,96                    // number of chars
        // -                LD A,3                      // table code (0=sprite name, 1=sprite gen, 2=pattern name, 3=pattern gen, 4=color)
        // -                CALL PUT_VRAM (0x1fbe->1C27)
        // -                    (This has lots of special cases, but I can just hard code the results)
        vdpmemcpy(0x0000+0x1d*8, (void*)FATFONT, 96*8);

        // -                LD HL,SPACE (0x15a3)        // also copies space into character #0
        // -                LD DE,0
        // -                LD IY,1
        // -                LD A,3                      // pattern table
        // -                CALL PUT_VRAM
        // -                RET
        vdpmemcpy(0x0000, (void*)(FATFONT+3*8), 8);      // space is after copyright, T, and M

        // and, although it's not in the startup code itself,
        // the Coleco BIOS leaves a color table at 0x2000 that we need to drop in
        vdpmemcpy(0x2003, colorTable, sizeof(colorTable));
    }

    // Don't use the stack or RAM variables anymore if we can help it. In particular there
    // are tables at 0x73b9 we mustn't mess with, or some carts will crash

    // after the above, NMI interrupts are disabled
    // -    load init code to RAM and continue from there
    // -    swap BIOS in
    // -    jump to cartridge startup vector
    // -        IN 0x55,A           DB55
    // -        LD HL,[0x800A]      2A800A
    // -        JP [HL]             E9
    memcpy((void*)0x6000, trampoline, sizeof(trampoline));
    ((void(*)())0x6000)();  // never returns
}

// start a hardware cartridge
void startCartridge() {
    // - set the mappers for real cart (should already be set)
    phBankingScheme = PH_BANK_CART | PH_UPPER_CART;

    // start the cart
    startTitle();
}

// display the cartridge copyright information, then start it
// This is an expensive function, but I like the results too much to remove it!
void runCartHeader() {
    // - read and display the cartridge name & copyright on the status line
    // String at 0x8024 is GAME_NAME/OWNER/COPYRIGHT
    // OWNER is displayed first and often includes "presents"
    // GAME NAME is on the second line and may include TM (0x1e, 0x1f)
    // COPYRIGHT is /always/ 4 digits. The font has a copyright symbal at 0x1d
    // 
    // Filter OWNER to remove "PRESENTS " (watch for leading spaces, watch for no trailing space before end of string (ie: /PRESENTS/)
    // 
    // Then do : OWNER GAME_NAME cYYYY
    // Remove leading, trailing, and duplicate spaces.
    // Split at 32 chars.
    // Remove leading and trailing spaces again, and center.
    // Adjust center for half space
    // we have a maximum of 32 chars per field.

    unsigned char *p = (unsigned char*)CART_NAME_INFO;
    unsigned char *o = copyright;
    const unsigned char *sk = "PRESENTS";
    unsigned char *pYr;
    unsigned char cntdown;

    // We keep finding exception cases that break this display, so to provide a workaround
    // for other carts, just check for held fire button here. If found, we'll skip the formatting
    readkeypad();   // read joystick fire buttons
    if (MY_KEY == JOY_FIRE) {     
        startCartridge();      // never returns
    };

    // clear the buffer
    memset(copyright, 0, sizeof(copyright));

    // treat any 0 bytes in the cart fields as space (ie: Video Hustler)

    // first find the owner and copy that out
    cntdown = 32;
    while ((*p != '/')&&(--cntdown > 0)) ++p;
    if (cntdown == 0) {
        // never mind - malformed string
        startCartridge();   // never returns
    }
    ++p;
    // skip leading spaces
    cntdown = 32;
    while ((*p == ' ')&&(--cntdown > 0)) {
        ++p;
    }

    // Skip the word 'PRESENTS' if present
    while ((*sk)&&(*sk == *p)) { ++p; ++sk; }

    // skip spaces again
    cntdown = 32;
    while ((*p == ' ')&&(--cntdown > 0)) {
        ++p;
    }

    // copy out - max output so far is 32
    cntdown = 32;
    while ((*p != '/')&&(--cntdown > 0)) {
        *(o++) = *(p++);
    }
    // save the pointer off for later
    pYr = p;

    // we know we could not have copied too many characters (33)
    *(o++)=' ';

    // okay, now we have "OWNER ", back up and get GAME_NAME
    p = (unsigned char*)CART_NAME_INFO;

    // skip leading spaces
    cntdown = 32;
    while ((*p == ' ')&&(--cntdown > 0)) {
        ++p;
    }

    // check how many characters we have left - we need to save
    // six for the " c1982" part (min count here is 24)
    cntdown = 63-(o-copyright)-6;

    // now just copy until the slash (max now is 57)
    while ((*p != '/')&&(--cntdown > 0)) {
        *(o++) = *(p++);
    }

    // now we are supposed to have enough room to finish it off
    // make sure we have the copyright year at pYr (we might have stopped early)
    cntdown = 32;
    while ((*pYr != '/')&&(--cntdown > 0)) {
        ++pYr;
    }
    if (*pYr == '/') {
        // we found it!
        ++pYr;

        *(o++) = ' ';
        *(o++) = 0x1d;  // (C)
        *(o++) = *(pYr++);
        *(o++) = *(pYr++);
        *(o++) = *(pYr++);
        *(o++) = *(pYr++);
        *(o) = '\0';        // total max of 63 printable, 64 bytes
    }

    // now from copyright to o-1, replace any 0 bytes with space
    // Predecrement here is important to avoid replacing the terminator
    while (--o >= copyright) {
        if (*o == 0) *o=' ';
    }

    // make positive it's NUL terminated, just in case
    copyright[64]='\0';
    
    // now we have the string, remove leading spaces
    while (copyright[0] == ' ') {
        memmove(copyright, copyright+1, sizeof(copyright)-1);
    }

    // search for and remove duplicate spaces
    p = copyright;
    while (*p) {
        if ((*p == ' ') && (*(p+1) == ' ')) {
            memmove(p, p+1, sizeof(copyright)-(p-copyright)-1);
        } else {
            ++p;
        }
    }

    // we should be able to just search backwards for the first non-zero byte
    p = copyright + sizeof(copyright) - 1;
    while ((p >= copyright) && (*p == '\0')) {
        --p;
    }

    // Okay, good. Conveniently, p now points to the end of the string, so check how long it is
    if (p-copyright > 31) {
        // we need to display a first string, find a space to truncate at
        p = copyright+30;
        while ((p > copyright) && (*p != ' ')) {
            --p;
        }
        // if, somehow, we didn't find a space, just assume it's corrupt and don't show anything
        // it shouldn't have happened!
        if (*p == ' ') {
            *p = '\0';
            centerString(0, copyright);
            ++p;
            centerString(1, p);
        }
    } else {
        // it all fits, use the lower line cause it looks better with the gap
        centerString(1, copyright);
    }

    // - wait 4 seconds, allow abort
    cntdown = 255;
    while (cntdown-- > 0) {
        readkeypad();   // read joystick fire buttons
        if (MY_KEY == JOY_FIRE) break;
        waitVblanks(1);
    }

    // - start cartridge
    startCartridge();
}

// This is where we start!
void main() {
	unsigned char x,idx;
    
    // disable interrupt processing
    VDP_INT_DISABLE;

    // we expect VRAM to be cleared out
    vdpmemset(0, 0, 16384);

    // I'm told we don't need to mute the AY or the SEX, and reset also turns off the SGM.

	// we must do this first - in case the user was running F18A software
	// and pressed reset, otherwise old settings will still be active.
    // NOTE: Phoenix F18A gets a reset on reset too, but it does NOT reset the palette
    reset_f18a();
    unlock_f18a();

    // ask the F18A to turn off the sprite limit
    VDP_SET_REGISTER(0x1e, 32); // stop sprite is #32
	
	// set_graphics leaves the screen disabled so the user can load character sets,
	// and returns the value to set in VDPR1 to enable it
    // Warning: in the menu.c we assume this value is 0xE2
	x = my_set_graphics(VDP_SPR_16x16);

    // clear the sprite pattern table (already done above)
//    vdpmemset(GSPRITEPAT, 0, 256*8);

	// set up 16 mag 3 sprites to give us two lines of bitmapped text at the bottom of the screen
    // this part won't work if the F18A isn't present, but I'm okay with that.
    // Why sprites? Oh, why not? Could use the BML but that's not implemented in Classic99
    // Debug is easier this way.
    VDP_SET_ADDRESS_WRITE(GSPRITE);
    for (idx=0; idx<16; ++idx) {
        // F18A doesn't need delays between writes
        VDPWD = (unsigned char)(22*8-1); // Y
        VDPWD = idx*16; // X
        VDPWD = idx*4;  // char
        VDPWD = 15;     // color
    }
    VDPWD = 0xd0;      // terminator
    
    // clear the screen
	vdpmemset(GIMAGE, 0x18, 768);
	// set the color table to white on transparent
	vdpmemset(GCOLOR, 0xF0, 32);

	// load the character patterns
    unrle(GPATTERN, PAT0, SIZE_OF_PAT0RLE);

	// turn the screen on
	VDP_SET_REGISTER(VDP_REG_MODE1, x);

	// wait for the first two vblanks
    waitVblanks(2);

    // test for a cartridge with the no logo requirement
    // note that startCartridge still does a lot of initialization,
    // in case it needs to undo the F18A setup for the menu later.
    if ((pRom(0x8000)==0x55)&&(pRom(0x8001)==0xaa)) {
        // give the user one chance to load the menu, if the cart autoboots
        readkeypad();
        // never returns
        startCartridge();
    }

	// F18A available 
	fadeInTextF18A();
    displayLogo();

    // Now that the fancy part is done, we can get to work on the real stuff.

    // Check for a cartridge plugged in with the logo header
    if ((pRom(0x8000)==0xaa)&&(pRom(0x8001)==0x55)) {
        runCartHeader();    // never returns
    }

    // If no cartridge
    // call the menu - never returns
    menu();
}

