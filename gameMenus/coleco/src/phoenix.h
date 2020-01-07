// definitions for Phoenix I/O ports
//

volatile __sfr __at 0x50 sgmPSGAddressWrite;    // PSG Address write
volatile __sfr __at 0x51 sgmPSGDataWrite;       // PSG Data write
volatile __sfr __at 0x52 sgmPSGDataRead;        // PSG Data read

volatile __sfr __at 0x53 sgmConfig;             // configuration
#define SGM_CFG_SGM_ENABLE 1
#define SGM_CFG_SEX_ENABLE 2

volatile __sfr __at 0x54 phRAMBankSelect;       // 32k RAM bank select. 0 and 1 are both 1.

volatile __sfr __at 0x55 phBankingScheme;       // cartridge emulation control (also read it to disable Phoenix BIOS)
#define PH_BANK_CART        0                 // hardware physical cart port (any other disables)
#define PH_BANK_MEGACART    1                 // banking scheme megacart
#define PH_BANK_SGM         2                 // banking scheme SGM (?)
#define PH_BANK_ATARI       3                 // banking scheme Atari (?)
#define PH_UPPER_INTRAM     0x00              // 32k RAM, banking ignored (uses port 0x54)
#define PH_UPPER_EXPROM     0x10              // banked cartridge emulation
#define PH_UPPER_EXPRAM     0x20              // banked RAM (same as cartridge emulation, I think, but RAM)
#define PH_UPPER_CART       0x30              // 32k fixed ROM

volatile __sfr __at 0x56 phSDControl;         // SD card control
#define PH_SD_CE_OFF        1                 // CE control, active low (RW)
#define PH_SD_LOW_SPEED     2                 // set for 400Khz, clear for 12MHz (RW)
#define PH_SD_CARD_DETECT   0x80              // read for card detect (other bits?)

volatile __sfr __at 0x57 phSDData;            // SD card read/write data
volatile __sfr __at 0x58 phMachineID;         // reads as 8 for Phoenix
volatile __sfr __at 0x59 phCartMask;          // cartridge page mask for megacarts 0x01-1F

volatile __sfr __at 0x7f sgmBIOSMap;          // swaps RAM with the Coleco BIOS
#define SGM_CFG_ENABLE_BIOS 0x02

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

// address that the Coleco BIOS font occurs, offset for the Phoenix memory map
//#define COLECO_FONT_ADR (0x158b + 0x4000)

void dly_50us(unsigned char n);

// Just some convenient loader defines to improve the code
// some tables that we never move - a constant will get better code out of SDCC
// Note you can't JUST change these, you need to fix the code that sets the registers too
#define GSPRITEPAT 0x1800
#define GSPRITE 0x0700
#define GCOLOR 0x1000
#define GIMAGE 0x0000
#define GPATTERN 0x0800



