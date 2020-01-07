// reset the F18A back to stock 9918A mode
void reset_f18a();
// unlock the F18A
void unlock_f18a();
// re-lock the F18A (1.6 or later)
// do this after the reset, if desired
void lock_f18a();
// load a palette into the F18A
// data format is 12-bit 0RGB (4 bits each gun)
void loadpal_f18a(const unsigned int *ptr, unsigned char cnt);

// prepare the F18A to launch a Coleco title with sprite and scanline settings
void prepare_f18a();

// define for the useScanlines variable
#define SCANLINES_ON 0x04
