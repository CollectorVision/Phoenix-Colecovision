// reduced functionality joystick and keyboard routines
// based on libti99coleco kscanfast and joystfast

#include "joykey.h"

#define SELECT 0x2a

// Address to read back the detected key. 0xFF if no key was pressed.
unsigned char MY_KEY;
// Address to read back the joystick X axis (scan modes 1 and 2 only)
unsigned char MY_JOYY;
// Address to read back the joystick Y axis (scan modes 1 and 2 only)
unsigned char MY_JOYX;

// note: keys index 8 and 4 are fire 2 and fire 3, respectively
// for now, I'm defining them the same as regular fire (18),
// but if I ever want to split up them, I can update this.
const unsigned char keys[16] = {
	0xff, '8', '4', '5', 
	0xff, '7', '#', '2',
	0xff, '*', '0', '9',
	'3',  '1', '6', 0xff
};
// FIRE 1 returns as bit 0x40 being low

static volatile __sfr __at 0xfc port0;
static volatile __sfr __at 0xff port1;
static volatile __sfr __at 0x80 port2;
static volatile __sfr __at 0xc0 port3;

// For Coleco, all modes except 2 read controller 1, and 2 reads controller 2
void readinputs(unsigned char mode) {
	unsigned char key;

	port2 = SELECT;		// select keypad

	if (mode == 2) {
		key = port1;
	} else {
		key = port0;
	}
	// bits: xFxxNNNN (F - active low fire, NNNN - index into above table)

	// if reading keypad, the fire button overrides
	// Note this limits us not to read keypad and fire2 at the same time,
	// which honestly I will probably want later.
	if ((key&0x40) == 0) {
		MY_KEY = JOY_FIRE;
	} else {
		MY_KEY = keys[key & 0xf];
	}

	MY_JOYX = 0;
	MY_JOYY = 0;

	port3 = SELECT;		// select joystick
	if (mode == 2) {
		key = port1;
	} else {
		key = port0;
	}
	// active low bits:
	// xFxxLDRU
	if ((key&0x40) == 0) {
		MY_KEY = JOY_FIRE;
	}
	if ((key&0x08) == 0) {
		MY_JOYX = JOY_LEFT;
	}
	if ((key&0x04) == 0) {
		MY_JOYY = JOY_DOWN;
	}
	if ((key&0x02) == 0) {
		MY_JOYX = JOY_RIGHT;
	}
	if ((key&0x01) == 0) {
		MY_JOYY = JOY_UP;
	}

}
