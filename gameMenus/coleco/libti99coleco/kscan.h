// KSCAN definitions for keyboard and joystick

//*********************
// KSCAN related addresses
//*********************

// Address to set the scan mode (see KSCAN_MODE_xxx defines) (not supported, pass to function)
//extern unsigned char KSCAN_MODE;

// Address to read back the detected key. 0xFF if no key was pressed.
extern volatile unsigned char KSCAN_KEY;

// Address to read back the joystick X axis (scan modes 1 and 2 only)
extern volatile unsigned char KSCAN_JOYY;

// Address to read back the joystick Y axis (scan modes 1 and 2 only)
extern volatile unsigned char KSCAN_JOYX;

// Address to check the status byte. KSCAN_MASK is set if a key was pressed
extern volatile unsigned char KSCAN_STATUS;
#define KSCAN_MASK	0x20

//*********************
// KSCAN modes
//*********************

#define KSCAN_MODE_LAST		0		// last mode scanned (except LEFT and RIGHT)
#define KSCAN_MODE_LEFT		1		// left side of keyboard and joystick 1 (fire is a key of 18)
#define KSCAN_MODE_RIGHT	2		// right side of keyboard and joystick 2 (fire is a key of 18)
#define KSCAN_MODE_994		3		// upper-case only, 99/4 compatible results
#define KSCAN_MODE_PASCAL	4		// PASCAL mapping, different control keys
#define KSCAN_MODE_BASIC	5		// Normal 99/4A BASIC mode


//*********************
// Joystick return values
//*********************

#define JOY_LEFT	0xfc
#define JOY_RIGHT	0x04
#define JOY_UP		0x04
#define JOY_DOWN	0xfc
#define JOY_FIRE	18


//*********************
// Function definitions
//*********************

// call the console SCAN function, supports all the regular keyboard modes, debounce, joysticks, etc.
// requires console ROM and GROM to be present. (not fully supported - we wrap to kscanfast instead)
unsigned char kscan(unsigned char mode);

// does a simple read of the keyboard with no shifts and no debounce. Mode 0 is keyboard,
// mode 1 is joystick 1 fire button (only!) and mode 2 is joystick 2 fire button (only!)
// Fire buttons are /not/ aliased to 'Q' and 'Y' on the keyboard. Returns key in KSCAN_KEY,
// no status. Key is 0xff if none pressed.
void kscanfast(unsigned char mode);

// read a joystick directly. 'unit' is either 1 or 2 for the joystick desired
// returns data in KSCAN_JOYY and KSCAN_JOYX
void joystfast(unsigned char unit);

