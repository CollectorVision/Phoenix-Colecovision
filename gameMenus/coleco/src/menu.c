//*****************************
//*****************************
// Phoenix Menu
// Relies on FatFS
// Called from PhoenixBoot
// Created by Tursi/M.Brent
// Uses libti99Coleco
// Note: we have 24k now per the new memory layout, and 8k RAM at 0x6000 (during loader only, first 1k is real Coleco RAM)
//
// Intentional limitations:
// - Maximum filename length of 126 characters.
// - Maximum of 254 files in a folder (including subfolders)
// - Maximum path length of 512 characters
// - 7-bit ASCII clean filenames only (ie: no accents or unicode characters)

#include "memset.h"
#include "vdp.h"
#include "phoenix.h"
#include "f18a.h"
#include "joykey.h"
#include "fatFs/ff.h"			/* Obtains integer types */
#include "fatFs/diskio.h"		/* Declarations of disk functions */

// --- externs ---

// fast memcpy, since we don't need delays on the F18A
extern void vdpmemcpyfast(int pAddr, const unsigned char *pSrc, int cnt);
#define vdpmemcpy vdpmemcpyfast

// kscan wrapper
extern void readkeypad();

// hardware control
extern void waitVblanks(unsigned char cnt);  // clear and wait for blanks
extern void startTitle();       // start with no init

// sprite layer updates (yes, text mode sprites!)
extern void textout(unsigned int vdp, const char *p);
extern void textouthalf(unsigned int vdp, const char *p);
extern void centerString(unsigned char line, char *p);

// Reference to the font for copying out of
extern const unsigned char FATFONT[];

// needed for the popup menu
extern unsigned char text_width;

// --- hardware ---

// This doesn't optimize correctly (it's always a calculated lookup), so just use a macro
//static volatile unsigned char * const pRom = (volatile unsigned char * const)0;	// access to memory
#define pRom(x) (*((volatile unsigned char*)(x)))


// --- definitions ---

// Color scheme - these will be redefined by palette below.
// To change colors, change the palette values rather than these numbers.
#define COLORLINE1      2
#define COLORLINE2      3
#define COLORTEXT       4
#define COLORDIRECTORY  5
#define COLORBG         6
#define COLORHILITE     7

// working RAM bank for directory - 0 is the SGM bank and can not
// be selected. 1 through 15 are the cartridge banks. However, this
// only gives us 480k. Unfortunately, Megacarts are powers of two,
// meaning we are restricted to 256k megacarts unless we can get
// that first 32k back. (But, we try to load 512k anyway, if the first
// 32k is unused, then we might just work.)
//
// I want to put the directory out of the way of file load, so that
// if an error occurs, we can go back to the directory without re-reading
// the SD card. So in case we do solve the above, I'll store the data
// at page 14. (Since we load page 1 during detection, then load up.)
#define DIRECTORY_PAGE 14
// cartridge pages (we need this stuff for the loader, defining it lets us
// change it easier later if we resolve the issue)
#define CART_FIRST_PAGE 1       // 0 is SGM, 1 is cart memory
#define CART_LAST_PAGE 15       // for 512k megacart (minus 32k)

// how long to sit idle before blanking (on no SD card or cartridge)
#define BLANK_TIME 5400    // about 3 minutes @ 30hz



// --- work buffers ---

// Current path, and a temporary for manipulating it
char path[512];
char path2[512];    // temporary ram for the load, since we can afford it... (size assumed by copypage() )

// options - mostly a temporary hack until the core menu is in place
// - stored in the F18A code
extern unsigned char useScanlines;     // takes in real time but we need it after a reset

// --- variables ---

// structures for file access
// Each entry has:
//      Filename[127]
//      Number of 16k pages (or 0 for directory)
// Using the 32k of RAM at 0x8000, we can fit 256. (32k/128)
// Alternate plans could be to use more than one page of
// data, or to pack the filenames rather than spacing
// them out 128 bytes each. (We could store a lookup
// table in VDP). But for now we'll just enforce the
// upper limit as a count.

unsigned char *lastFilename=0;      // points after last RAM filename entry address
unsigned char *sortedList[256];     // sorted pointers to filenames
unsigned char listSize = 0;         // number of elements in sortedList
unsigned char listOffset = 0;       // current list offset
unsigned char listSelect = 0;       // current selection index
unsigned char repeatTimeout;        // number of frames before autorepeat
unsigned char firstScan = 0;        // during the first scan, we look for a Coleco folder and enter it
unsigned char firstDir = 0;         // indicates we need to set up the display for the first time showing a directory

// File structures
FATFS fatFs;
FIL fil;
DIR dir;
FILINFO finfo;

// --- static data ---

// foldername to search for - MUST be uppercase, no numbers or punctuation
const unsigned char FolderName[] = "COLECO";
const unsigned char FolderNameLen = 7;      // include the terminating NUL

// color scheme for menu - this allows easy customization
// we can use an F18A palette to make these any colors we want
// COLORHILITE should pulse a little using palette color cycling
// menu F18 palette values - 12 bit format 0000RRRR GGGGBBBB
const unsigned int MENU_PALETTE[] = {
	0x0000,0x0000,  // unchanged transparent and black just for comfort (white is also unchanged)
    0x0000,         // line 1
    0x0010,         // line 2
    0x0eee,         // text
    0x044e,         // directory
    0x0000,         // background
    0x0900          // hilite
    // we don't need to define the rest
};

// black and white high contrast palette
const unsigned int MENU_CONTRAST[] = {
	0x0000,0x0000,  // unchanged transparent and black just for comfort (white is also unchanged)
    0x0000,         // line 1
    0x0000,         // line 2
    0x0eee,         // text
    0x0aaa,         // directory
    0x0000,         // background
    0x0222          // hilite
    // we don't need to define the rest
};

// black and white inverted palette
const unsigned int MENU_INVERT[] = {
	0x0eee,0x0eee,  // match to new background
    0x0eee,         // line 1
    0x0eee,         // line 2
    0x0000,         // text
    0x0444,         // directory
    0x0eee,         // background
    0x0ccc          // hilite
    // we don't need to define the rest
};

const unsigned int *MENU_COLOR_SETS[] = {
	MENU_PALETTE,
	MENU_INVERT,
	MENU_CONTRAST
};

unsigned char currentPalette;

// hex conversion
const char hex[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };

// --- and functions... ---

// each index is about 50us
void dly_50us(unsigned char n)
{
    // roughly 50us per loop
    // must be an even number of ex's to
    // not mess up the stack or HL
    while (n--) {
        __asm
            ex (sp),hl      ; 19 t-states
            ex (sp),hl      ; 19 t-states
            ex (sp),hl      ; 19 t-states
            ex (sp),hl      ; 19 t-states
            ex (sp),hl      ; 19 t-states
            ex (sp),hl      ; 19 t-states
            ex (sp),hl      ; 19 t-states
            ex (sp),hl      ; *8 = 152 t-states =~ 42.4uS, plus loop overhead
        __endasm;
    }
}

// clear the sprite layer
void cleartextout() {
    vdpmemset(GSPRITEPAT, 0, 16*4*8);
}

// clear the text screen
void clrscr() {
    vdpmemset(GIMAGE, ' ', 960);
}

// clear the text screen bottom three rows
void clrscrbottom() {
    vdpmemset(GIMAGE+(40*21), ' ', (40*3));
    cleartextout();
}

// output a string at current location, stopping at end of string or end of cnt
void vdpStrOut(const char *p, unsigned char cnt) {
    ++cnt;  // account for pre-decrement so we stop at 0
    while ((*p >= ' ') && (--cnt)) {
        VDPWD = *(p++);
    }
    if (cnt) {
        while (--cnt) {
            VDPWD = ' ';
        }
    }
}

// strcpy with max length for our directory entries
void mystrcpy(char *pDest, char *pSrc) {
    unsigned char cnt = 126;
    while ((*pSrc) && (cnt--)) {
        *(pDest++)=*(pSrc++);
    }
    *(pDest) = 0;
}

// Wait for both player 1 and player 2 fire buttons to be released
// (actually any keys)
void waitBothFireClear() {
    for (;;) {
        readkeypad();
        if (MY_KEY == 0xff) break;
    }
}

// wait for player 1 or player 2 fire button to be pressed
// (fire ONLY)
void waitAnyFirePressed() {
    for (;;) {
        readkeypad();
        if (MY_KEY == JOY_FIRE) break;
    }
}

// Blank the screen and wait for an input, then restore the screen
void handleBlanking(unsigned char mode) {
    unsigned char disk = disk_status();

    // blank the screen, cause all 9918 systems seem to
    VDP_SET_REGISTER(VDP_REG_MODE1, mode&(~0x40));

    // wait for any input to clear it
    for (;;) {
        readkeypad();
        if (MY_KEY != 0xff) break;
        if ((MY_JOYY)||(MY_JOYX)) break;

        // even SD insert or remove - no need to sleep here
        if (disk_status() != disk) break;
    }

    // re-enable the screen
    VDP_SET_REGISTER(VDP_REG_MODE1, mode);  // as above, plus unblank
}

// use the sprite layer to display an error at the bottom of the screen
void displayErrorString(char *pstr, unsigned char x) {
    // - Clear bottom of screen (for text mode)
    clrscrbottom();
    centerString(0, pstr);
    centerString(1, "Press fire...");

    // write the code 'x' to top left
    if (firstDir) {
        VDP_SET_ADDRESS_WRITE(GIMAGE + 30);
        VDPWD = x>>4;
        VDPWD = x&0x0f;
    } else {
        VDP_SET_ADDRESS_WRITE(GIMAGE + 38);
        VDPWD = hex[(x>>4)];
        VDPWD = hex[(x&0xf)];
    }

    // make sure both fires are released
    waitBothFireClear();

    // wait for a new fire button
    waitAnyFirePressed();

    // clear sprites - caller should redraw the text layer
    cleartextout();

    // make sure both fires are released
    waitBothFireClear();
}

// case-insensitive strcmp (char return instead of int cause z80)
// also treats underscore as equal to space
char mystrcmp(const char *s1, const char *s2) {
    unsigned char c1,c2;
    for (;;) { 
        c1 = *s1;
        c2 = *s2;

        // make uppercase (with some sideeffects, but not in the valid charset)
        // and turn underscores into spaces for better sorting

        if (c1&0x40) {
            c1&=0xdf;
        } else if (c1 == '_') { // if it's a letter, it can't be an underscore, so 'else'
            c1=' ';
        }

        if (c2&0x40) {
            c2&=0xdf;
        } else if (c2 == '_') {
            c2=' ';
        }

        // compare
        if (c1 != c2) break;
        if (c1 == 0) break;  // only need this test if they are BOTH NUL

        // next character
        ++s1; 
        ++s2; 
    }
    return (c1-c2);
}

// read the directory ('path') into expansion RAM starting at 0x8000
// this RAM is currently free for use, when we load a cartridge, then
// it will get overwritten. We use a page that isn't overwritten until
// the cartridge load is successful.
// The first 126 bytes are the filename, always followed by a NUL byte,
// then one byte for the number of 16k pages in the file, which is
// zero if it's a directory.
void readDir() {
repeatDir:
    lastFilename = (unsigned char*)0x8000;  // no files loaded yet

    // Each entry has:
    //      Filename[127]
    //      Byte for size (/16k) or directory
    if (FR_OK == f_opendir(&dir, path)) {
        // To avoid corruption caused by centerString, 
        // we use path2 as a temporary (it's safe at this point)
        mystrcpy(path2, path);
        centerString(1, path2);

        if (path[1] != 0) {
            // in a subfolder, so add the ..
            memset(lastFilename, 0, 128);
            lastFilename[0]='.';
            lastFilename[1]='.';
            lastFilename[127]=0;    // directory flag
            lastFilename+=128;
        }

        while (FR_OK == f_readdir(&dir, &finfo)) {
            // just in case the return code wasn't enough
            if (disk_status() == STA_NODISK) {
                f_closedir(&dir);
                return;
            }

            {
                unsigned char x = finfo.fname[0];

                if (finfo.fname[0] == '.') {
                    continue;   // ignore files starting with period
                }

                if (finfo.fname[0] == '\0') {
                    break;      // end of directory
                }
            }

            if ((firstScan) && (finfo.fattrib & AM_DIR)) {
                // Look for a Coleco folder - this will slow down the first read a bit...
                // but it was requested to auto-enter it since people will probably have
                // one card with all their systems on it.
                // this could cause issues if we reopen a smaller pathname, but
                // the only case we should ever follow this is going from "/" to "/Coleco"
                if (0 == mystrcmp(finfo.fname, FolderName)) {
                    f_closedir(&dir);
                    memcpy(&path[1], FolderName, FolderNameLen);
                    firstScan = 0;
                    goto repeatDir;
                }
            }
            
            // save it off
            mystrcpy(lastFilename, finfo.fname);    // will truncate to 126 characters
            if (finfo.fattrib & AM_DIR) {
                *(lastFilename+127) = 0;            // 0 size for directory
            } else {
                *(lastFilename+127) = (finfo.fsize + 16383) >> 14;  // number of 16384 pages, rounded up
            }
            lastFilename += 128;
            // we can go up to 0xffff, but if we wrap around we're done
            // to be paranoid for now, I'm going to stop at 254
            if (lastFilename >= (unsigned char*)0xff80) {
                displayErrorString("Max directory count reached", (unsigned char)(((unsigned int)lastFilename-0x8000)>>7));
                break;
            }
        }

        f_closedir(&dir);
        firstScan = 0;
    }
}

// wait for joystick key to be released
// there's a timeout to provide a crude auto-repeat
void waitRelease() {
    for (;;) {
        readkeypad();
        if ((MY_JOYY == 0) && (MY_JOYX == 0)) {
            repeatTimeout = 45;     // hold this long before next repeat starts
            break;
        } else {
            --repeatTimeout;
            if (repeatTimeout == 0) {
                repeatTimeout = 5;  // faster repeat if you keep holding
                break;
            }
            // VDP vblank wait
            waitVblanks(1);
        }
    }
}

// display the list of titles
void drawTitles() {
    unsigned char idx;
    unsigned int vdp = GIMAGE;
    unsigned int atvdp = GCOLOR;

    for (idx = listOffset; idx < listOffset+24; ++idx) {
        unsigned char *ptr;

        if (idx >= listSize) {
            // draw a blank line with text attribute
            if (idx&1) {
                vdpmemset(atvdp , (COLORTEXT<<4) | COLORLINE2, 40);
            } else {
                vdpmemset(atvdp , (COLORTEXT<<4) | COLORLINE1, 40);
            }
            atvdp+=40;
            // write a blank line
            VDP_SET_ADDRESS_WRITE(vdp);
            vdp+=40;
            vdpStrOut("", 40);
            // dealing with unsigned char limit is faster here than
            // upscaling it to a 16-bit int
            if (idx == 0xff) {
                idx--;      // so the loop doesn't wrap around
                if (vdp == 24*40) break;
            }
        } else {
            ptr = sortedList[idx];

            if (ptr[127] == 0) {
                // change the attributes to include directory
                if (idx&1) {
                    vdpmemset(atvdp , (COLORDIRECTORY<<4) | COLORLINE2, 40);
                } else {
                    vdpmemset(atvdp , (COLORDIRECTORY<<4) | COLORLINE1, 40);
                }
            } else {
                // text attributes (we have to rewrite it in case it was previously a dir)
                if (idx&1) {
                    vdpmemset(atvdp , (COLORTEXT<<4) | COLORLINE2, 40);
                } else {
                    vdpmemset(atvdp , (COLORTEXT<<4) | COLORLINE1, 40);
                }
            }
            atvdp+=40;

            // this will stop at the directory flag byte or end of the line
            // TODO: we should scroll the currently selected title so long filenames are readable
            VDP_SET_ADDRESS_WRITE(vdp);
            vdp+=40;
            vdpStrOut(ptr, 40);
        }
    }
}

// uses an insertion sort to create a list of pointers to the discovered files, in alphabetical order
void sortDir() {
    // Hooray for lots of RAM all of a sudden!
    // Since we are building the sort from scratch, we can
    // use a per-element insertion rather than worrying about
    // being fancy.
    // Extra rules: directories before non-directories
    unsigned char *ptr = (unsigned char*)0x8000;
    listSize = 0;

    while (ptr < lastFilename) {
        unsigned char idx;
        // whereever this loop exits, that's where we store it

        // This is orders of magnitude faster on a mostly-sorted folder
        // Two loops here - if we are a file, we work backwards,
        // and a directory, we work forwards. This is just to optimize
        // the search performance a little. Files are generally
        // mostly sorted.
        if (*(ptr+127) == 0) {
            // directory, so work from the beginning
            for (idx = 0; idx < listSize; ++idx) {
                unsigned char *ptr2;

                // check against the already sorted code
                ptr2 = sortedList[idx];

                // if we are a directory comparing to a file, stop here
                if ((*(ptr2+127)) != 0) break;

                // otherwise, if we have a lower filename, stop here
                // equal shouldn't happen, but if it does it will sort after
                if (mystrcmp(ptr, ptr2) < 0) break;
            }
        } else {
            // file, so work from the end
            for (idx = listSize-1; idx != 255; --idx) {
                unsigned char *ptr2;

                // check against the already sorted code
                ptr2 = sortedList[idx];

                // if we are a file comparing to a directory, break
                if ((*(ptr2+127)) == 0) {
                    ++idx;
                    break;
                }

                // otherwise, if we have a higher filename, continue
                // equal shouldn't happen, but if it does it will sort after
                if (mystrcmp(ptr, ptr2) < 0) {
                    continue;
                }

                // we're done!
                ++idx;
                break;
            }
        }

        // if necessary, move the data out of the way
        if (idx < listSize) {
            memmove(&sortedList[idx+1], &sortedList[idx], (listSize-idx)*2);
        }
            
        // store this element
        sortedList[idx] = ptr;

        // the list will now be larger
        ++listSize;

        // next filename
        ptr += 0x80;
    }
}

// wait for an SD card to be inserted (with screen blank)
void waitSDCard() {
    int cntDown;    // used to count down to screen blank timeout
    unsigned char debounce; // make sure card detect is stable if we see insertion

    // - If card can't be accessed:
    // -    put up     Insert SD or turn system
    //              off before inserting cartridge
                    // row    col
    cleartextout();
    textout(GSPRITEPAT+(0*8)+(4*16), "Insert SD or turn system");
    textout(GSPRITEPAT+(1*8)+(1*16), "off before inserting cartridge");

    cntDown = BLANK_TIME;
    debounce = 0;
    while (debounce < 2) {
        // -    Check for SD card periodically
        // -    When detected, jump back to the 'reading SD card' line
        waitVblanks(2);

        // countdown screen timeout
        if (--cntDown == 0) {
            handleBlanking(0xe2);
            cntDown = BLANK_TIME;
            debounce = 0;
        }

        // check for and debounce SD card detect
        if (disk_status() == STA_NODISK) {
            debounce = 0;
        } else {
            ++debounce;
        }

        // allow a chance for the menu
        readkeypad();
    }
}

// read the rest of a 32k cart (The first 512 bytes are loaded)
// any return is an error. FILE object is in global fil.
void loadCartridgeRom() {
    unsigned char *p = (unsigned char*)(0x8000+512);
    UINT br;

    clrscrbottom();
    centerString(0, "reading cart...");

    // - read in the rest
    for (;;) {
        FRESULT res;

        res = f_read(&fil, p, 512, &br);
        if (res != FR_OK) {
            f_close(&fil);
            displayErrorString("Cartridge load failed.", res);
            return;
        }
        if (br < 512) {
            // end of file
            break;
        }
        // else, keep reading
        p+=512;
        if (p < (unsigned char*)0x8000) {
            // if it's exactly 32k, we get here. 
            if (f_eof(&fil)) {
                break;
            } else {
                f_close(&fil);
                displayErrorString("Unrecognized >32k Cart", 1);
                return;
            }
        }
    }
    f_close(&fil);

    // - set the mappers appropriately
    phBankingScheme = PH_BANK_MEGACART | PH_UPPER_CART;     // the megacart banking is ignored for upper type cart

    // - jump to "execute title" as above
    startTitle();   // never returns
}

// copy 32k from phoenix page a to phoenix page b
void copyPage(unsigned char a, unsigned char b) {
    // since we already have loaded the program by this point,
    // let's reuse path2 as a 512 byte buffer
    unsigned char *off = (unsigned char*)0x8000;
    while (off > 0) {   // watch for wrap around
        phRAMBankSelect = a;
        memcpy(path2, off, 512);
        phRAMBankSelect = b;
        memcpy(off, path2, 512);
        off += 512;
    }
}

// reads the rest of a Megacart into memory
// fsize is the number of 16k blocks the cart contains
// note it's likely both the initial block reads are wasted
// on return (failure), we return the page we reached, which
// lets us decide whether to reload the directory
unsigned char loadMegacartRom(unsigned char fsize) {
    unsigned char *p;
    unsigned char page;
    UINT br;
    FSIZE_t ofs;

    clrscrbottom();
    centerString(0, "reading megacart...");

    // seek offset. In the case of a 512k image,
    // we have to skip the first 32k and hope for the best.
    ofs = 0;
    if (fsize == 32) {
        ofs = (unsigned int)32*1024;
        fsize-=2;
    }

    // calculate the first page - pages are 32k each so divide by 2
    page = CART_LAST_PAGE - ((fsize-1)>>1);
    phRAMBankSelect = page;

    // calculate the starting address, it's just the MOD of above
    // although for any valid image it's going to always be 0x8000+0.
    p = (unsigned char*)0x8000 + ((fsize&1)?0x4000:0);

    // seek back as requested (usually to 0)
    if (FR_OK != f_lseek(&fil, ofs)) {
        f_close(&fil);
        displayErrorString("Failed to seek file.", 2);
        return page;
    }

    // - read in the cart as specified above
    for (;;) {
        FRESULT res;

        res = f_read(&fil, p, 512, &br);
        if (res != FR_OK) {
            f_close(&fil);
            displayErrorString("Cartridge load failed.", res);
            return page;
        }
        if (br < 512) {
            // end of file
            break;
        }
        // else, keep reading
        p+=512;
        if (p < (unsigned char*)0x8000) {
            // time for the next page!
            ++page;
            if (page > CART_LAST_PAGE) {
                // if it's exactly 512k, we get here.
                if (f_eof(&fil)) {
                    // end of file
                    break;
                } else {
                    // we ran out of space
                    f_close(&fil);
                    displayErrorString("Megacart ROM too large", 0);
                    return page;
                }
            }
            // else, reset the pointer and keep reading
            p = (unsigned char*)0x8000;
            phRAMBankSelect = page;
        }
    }

    f_close(&fil);

    // instead of the fixed size approach, we just set a register mask
    // Valid sizes then are 32k, 64k, 128k, 256k, 512k, so fsize is:
    //                       2    4    8    16    32 
    // The question is whether we want to support oddball sizes. We
    // could TRY to here...
    switch (fsize) {
        case 2: 
            phCartMask = 0x01;  // 2 pages
            break;

        case 4: 
            phCartMask = 0x3;   // 4 pages
            break;

        case 8: 
            phCartMask = 0x07;  // 8 pages
            break;
        
        case 16:
            phCartMask = 0x0f;  // 16 pages
            break;

        case 30:    // should be 32, but we hacked it up
            phCartMask = 0x1f;  // 32 pages
            break;

        default:
            f_close(&fil);
            displayErrorString("Megacart images must be padded", fsize);
            return page;
    }

    // - set the mappers appropriately
    phBankingScheme = PH_BANK_MEGACART | PH_UPPER_EXPROM;
    phRAMBankSelect = CART_FIRST_PAGE;  // this is not necessarily enforced on real megacarts

    // - jump to "execute title" as above
    startTitle();   // never returns

    // should be unreachable, but SDCC complains
    return 0;
}

// draw the highlight on the current selection
void drawSelect() {
    unsigned int adr;
    unsigned char attr;

    if (listSelect > listSize-1) {
        listSelect = listSize-1;
    }

    adr = listSelect - listOffset;     // row on screen
    adr *= 40;      // multiply for offset
    adr += GCOLOR;  // index into attribute table

    attr = vdpreadchar(adr);            // get the current attribute (for text color)
    attr = (attr&0xf0) | COLORHILITE;    // highlight the background
    
    vdpmemset(adr, attr, 40);           // write out the whole line the same
}

// erase the highlight on the current selection
void undrawSelect() {
    unsigned char bg;
    unsigned int adr;
    unsigned char attr;

    adr = listSelect - listOffset;     // row on screen
    // while we have the row, calculate which background to use
    if (adr & 1) {
        // odd row
        bg = COLORLINE2;
    } else {
        bg = COLORLINE1;
    }
    adr *= 40;      // multiply for offset
    adr += GCOLOR;  // index into attribute table

    attr = vdpreadchar(adr);   // get the current attribute (for text color)
    attr = (attr&0xf0) | bg;    // highlight the background
    vdpmemset(adr, attr, 40);  // write out the whole line the same
}

// handle the user input to the menu
// returns:
//   0 = card was removed
//   1 = user made a selection
//  -1/-2  = user paged the directory (redraw needed, indicates which stick)
char getUserSelection() {
    int cntDown = BLANK_TIME;

    // draw the selection bar
    drawSelect();

    // start the loop
    for (;;) {
        unsigned char oldSelect;
        // if we lose the SD card, abort
        if (disk_status() == STA_NODISK) {
            return 0;
        }

        // VDP vblank wait
        waitVblanks(1);

        // so we can check if we moved
        oldSelect = listSelect;

        // scan the joysticks (both work)
        readkeypad();

        if (MY_JOYY == JOY_UP) {
            if (listSelect > 0) {
                undrawSelect();     // clear the highlight
                --listSelect;       // count down
                if (listSelect < listOffset) {
                    listOffset--;
                    return -1;      // request redraw
                }
                drawSelect();
            }
            waitRelease();
            cntDown = BLANK_TIME;
            continue;
        }

        if (MY_JOYY == JOY_DOWN) {
            if (listSelect < listSize-1) {
                undrawSelect();
                ++listSelect;
                if (listSelect > listOffset+23) {
                    listOffset++;
                    return -1;      // request redraw
                }
                drawSelect();
            }
            waitRelease();
            cntDown = BLANK_TIME;
            continue;
        }

        if (MY_JOYX == JOY_RIGHT) {
            if (listOffset+23 < listSize-1) {
                // page ahead, restrict to 8-bits
                unsigned char add = listSize-1-listOffset;
                if (add < 24) {
                    listOffset += add;
                    listSelect = listOffset;
                } else {
                    listOffset += 24;
                    listSelect += 24;
                }
                return -1;          // request redraw
            } else {
                undrawSelect();
                listSelect = listSize-1;
                drawSelect();
            }
            waitRelease();
            cntDown = BLANK_TIME;
            continue;
        }

        if (MY_JOYX == JOY_LEFT) {
            if (listOffset > 0) {
                // page back, restrict to 8 bits
                if (listOffset < 24) {
                    listOffset = 0;
                    listSelect = 0;
                } else {
                    listOffset -=24;
                    listSelect -=24;
                }
                return -1;          // request redraw
            } else {
                undrawSelect();
                listSelect = 0;
                drawSelect();
            }
            waitRelease();
            cntDown = BLANK_TIME;
            continue;
        }

        // check fire button
        if (MY_KEY == JOY_FIRE) {
            return 1;   // selected
        }

        if (oldSelect == listSelect) {
            // no joystick move, so reset the auto-repeat
            // a little hacky here, but it will work.
            repeatTimeout = 45;
        }

        // check if it's time to blank the screen
        --cntDown;
        if (cntDown <= 0) {
            // yes, it is
            handleBlanking(0xf2);
            cntDown = BLANK_TIME;
            continue;
        }

        // and repeat
    }
}

// load the selected palette
void load_palette() {
	loadpal_f18a(MENU_COLOR_SETS[currentPalette], 8);
}

void inc_palette() {
	++currentPalette;
	if (currentPalette >= sizeof(MENU_COLOR_SETS)/sizeof(MENU_COLOR_SETS[0])) currentPalette = 0;
	load_palette();
}


// run the menu itself
// this function is long and uses gotos. Don't look if you're academic in nature :)
void menu() {
    unsigned char *p;

    // check if the SD card is present
nocard:
    cleartextout();

    if (disk_status() == STA_NODISK) {
        waitSDCard();
        // on return, we must have an SD card to read!
    }

    // - Set directory to root (note there are no relative directories!)
    path[0]='/';
    path[1]=0;
    if (FR_OK != f_mount(&fatFs, "", 1)) {
        displayErrorString("Failed to mount SD",0);
        goto nocard;
    }
    firstScan = 1;
    firstDir = 1;

dirLoop:
    // - put up 'reading SD card' on status line
    cleartextout();
	textouthalf(GSPRITEPAT+(0*8)+(12*16), "READING");
	
    // activate RAM at the cartridge space so we have somewhere to store data
    phBankingScheme = PH_BANK_MEGACART | PH_UPPER_INTRAM;
    // select the working memory for the directory
    phRAMBankSelect = DIRECTORY_PAGE;

    // - Read directory (with long filenames), store filenames in VDP
    readDir();
    if (disk_status() == STA_NODISK) goto nocard;
    if (lastFilename == (unsigned char*)0x8000) {
        // then we failed to open the subdirectory, cause at least the ".." should have been created
        displayErrorString("Failed to open directory", 0);
        goto nocard;    // start right over at the root
    }

    // - Clear sprites
    // - Sort list
    // Sort is so fast now that we don't need to display anything
    // sortDir will set listSize
    sortDir();

    // If there are no files, then probably something went wrong, or the card is really empty.
    // We'll loop around and retry till the user replaces the card
    if (listSize == 0) {
        centerString(1, "No files found");
        goto nocard;    // start right over at the root
    }

    // clear any pending text
    cleartextout();

    // start at the top
    listOffset = 0;     // top of display
    listSelect = 0;     // user selected index

    if (firstDir) {
        firstDir = 0;

        // turn off the screen
        VDP_SET_REGISTER(1, 0x80);

        // - Load default font
        vdpmemcpy(0x0800+29*8, FATFONT, 94*8);

        // set up for per-character attributes (the color table is now a per-character color in text mode)
//        VDP_SET_REGISTER(50,0x02);
        useScanlines |= 0x02;
        VDP_SET_REGISTER(50,useScanlines);

        // background color to match the color scheme
        VDP_SET_REGISTER(7, COLORBG);

        // - Clear screen (for text mode)
        clrscr();

        // load the new palette
        currentPalette = 0;
        load_palette();

        // set text mode
        VDP_SET_REGISTER(1,0xf2);
        text_width = 40;
    } else {
        clrscr();
    }

drawLoop:
    // - Display list of titles
    drawTitles();

    // - wait for user selection (up/down move, left/right page, fire select)
    for (;;) {
        char ret = getUserSelection();
        if (ret >= 0) {
            // either user selected, or card was removed
            break;
        }

        // and if the user paged (or scrolled), then redraw
        drawTitles();

        // and then wait for the autorepeat
        waitRelease();
    }

    // in the event that the SD card was removed, jump back up
    if (disk_status() == STA_NODISK) {
        // - Clear screen (for text mode)
        clrscr();
        // go back to nocard loop
        goto nocard;
    }

    // We now have a selection index in listSelect
    // listSize is the number of entries, and lastFilename
    // points after the last entry
    p = sortedList[listSelect];

    // - if selection is directory:
    if (*(p+127) == 0) {
        unsigned char *pp;
        unsigned char *originalP;

        // - set new path
        pp = path;
        while (*pp) ++pp;   // find the end of the current string
        originalP = pp;     // in case we need to undo it

        // check for '..' and go back a directory if it's that
        if ((p[0]=='.')&&(p[1]=='.')) {
            while ((*pp != '/') && (pp > path)) --pp;
            if (pp == path) {
                // something went wrong, so rewrite the root dir
                path[0]='/';
                path[1]=0;
            } else {
                // NUL terminate at the /
                *pp = 0;
            }
        } else {
            // - copy the path in - watch for the end
            if (pp > path+1) {
                *(pp++) = '/';
            }

            // We probably need to use a shorter length here, since there's no filename room...
            while ((*p) && (pp-path < 510)) {   // this 510 means we always have room for the '/'
                *(pp++) = *(p++);
            }

            // see if we ran out of space
            if (*p) {
                // we need to tell the user some how, and revert to the original path
                pp = originalP;
                *pp = 0;
                displayErrorString("Path too long...", 0);
                goto drawLoop;
            }

            // and then NUL terminate it
            *(pp++) = 0;
        }

        // - Clear screen (for text mode)
        clrscr();

        // - display 'reading SD card' in center of screen
        // - jump back to directory read
        goto dirLoop;
    }

    // if we get here, then we're selecting a file
    // first switch to the first cartridge 32k bank -
    // if we fail to load, we can switch back to the
    // directory bank and keep working without a reload

    {
        // first create a filename to open
        FRESULT res;
        UINT br;
        char *s1, *s2;
        char fsize = *(p+127);  // save off the size (16k chunks)

        // create the filename
        s1 = path2;
        s2 = path;
        
        // we know the first one must fit, they are the same size
        while (*s2) {
            *(s1++) = *(s2++);
        }
        
        // also, the directory code guarantees room for the '/'
        // don't add it if we're in the root directory though
        if (path[1] != 0) {
            *(s1++) = '/';
        }
        
        // now, for the actual filename we have to watch length
        while ((*p) && (s1-path2 < 127)) {
            *(s1++) = *(p++);
        }
        *s1 = 0;

        // did we run out of room?
        if (*p) {
            displayErrorString("Path too long...", 1);
            goto drawLoop;
        }

        // all right, we have a full path, try to open it
        clrscrbottom();
        centerString(0, "checking...");

        res = f_open(&fil, path2, FA_READ);
        if (FR_OK != res) {
            displayErrorString("Failed to open file.", res);
            goto drawLoop;
        }

        // -    read first block into cartridge space
        // Remember after this point to restore the bank select before
        // looping back to drawLoop.
        phRAMBankSelect = CART_FIRST_PAGE;

        res = f_read(&fil, (unsigned char*)0x8000, 512, &br);
        if ((res != FR_OK) || (br != 512)) {
            f_close(&fil);
            displayErrorString("Unrecognized file type.", res);
            phRAMBankSelect = DIRECTORY_PAGE;
            goto drawLoop;
        }

        // -    if 55AA or AA55 is detected, we have a valid ROM:
        if ( ((pRom(0x8000) == 0xaa) && (pRom(0x8001) == 0x55)) ||
             ((pRom(0x8000) == 0x55) && (pRom(0x8001) == 0xaa)) ) {
            loadCartridgeRom();
            // if we return, then the cartridge load failed
            // 32k carts can't overwrite our directory, so just resume!
            phRAMBankSelect = DIRECTORY_PAGE;
            goto drawLoop;
        }

        // -    read EOF-16k block to next free memory, as test
        // Note that if the ROM is not a multiple of 16k, this test
        // probably won't work
        {
            FSIZE_t ofs;

            ofs = fsize - 1;    // get the size in blocks, minus 1
            ofs <<= 14;         // times 16384 for offset
            if (FR_OK != f_lseek(&fil, ofs)) {
                f_close(&fil);
                displayErrorString("Failed to identify file.", res);
                phRAMBankSelect = DIRECTORY_PAGE;
                goto drawLoop;
            }

            // this becomes a redundant read, but it's quick enough to be ok
            res = f_read(&fil, (unsigned char*)0x8200, 512, &br);
            if ((res != FR_OK) || (br != 512)) {
                // the br != 512 MIGHT be legal, but I'm going to say it should be padded to 16384!
                f_close(&fil);
                displayErrorString("Failed to read block.", res);
                phRAMBankSelect = DIRECTORY_PAGE;
                goto drawLoop;
            }

            // -    if 55AA or AA55 is detected, we have a megacart:
            if ( ((pRom(0x8200) == 0xaa) && (pRom(0x8201) == 0x55)) ||
                 ((pRom(0x8200) == 0x55) && (pRom(0x8201) == 0xaa)) ) {

                // if we return, then the cartridge load failed
                if (loadMegacartRom(fsize) >= DIRECTORY_PAGE) {
                    // the load potentially corrupted the directory, so reload it
                    goto dirLoop;
                } else {
                    // the directory should be safe!
                    phRAMBankSelect = DIRECTORY_PAGE;
                    goto drawLoop;
                }
            }
        }

        // we don't know what we found!
        f_close(&fil);
        displayErrorString("Unrecognized file type.", 0);
        phRAMBankSelect = DIRECTORY_PAGE;
        goto drawLoop;
    }

    // TODO: other cartridge banking types? Megacart only for now.
}
