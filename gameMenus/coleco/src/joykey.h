// reduced functionality joystick and keyboard routines
// based on libti99coleco kscanfast and joystfast

// Address to read back the detected key. 0xFF if no key was pressed.
extern unsigned char MY_KEY;
// Address to read back the joystick X axis (scan modes 1 and 2 only)
extern unsigned char MY_JOYY;
// Address to read back the joystick Y axis (scan modes 1 and 2 only)
extern unsigned char MY_JOYX;

//*********************
// Joystick return values
//*********************
#define JOY_LEFT	0xfc
#define JOY_RIGHT	0x04
#define JOY_UP		0x04
#define JOY_DOWN	0xfc
#define JOY_FIRE	18

// read inputs - return in above variables.
// 1 for controller 1, 2 for controller 2
void readinputs(unsigned char mode);