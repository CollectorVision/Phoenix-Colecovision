# BIN2PHX

Phoenix PHX core file mastering.


## Usage

Ensure that the multiboot features and BIN file creation are enabled on the "Generate Programming File" process properties (“Property display level“ must be set to “Advanced”):

* Check “Enable Cyclic Redundancy Checking (CRC)”
* Check “Retry Configuration if CRC Error Occurs”
* Set “Watchdog Timer Value” to 0x1FFF
* "Place MultiBoot Settings into Bitstream” MUST NOT be checked! (these are already included and applied on the boot sector)
* Set “Other Bitgen Command Line Options” to “-g next_config_register_write:Disable”
* Make sure the “Set SPI Configuration Bus Width” is set to 1
* Check "Create Binary Configuration File"

Bit2Bin can also be used on an already existing BIT file to obtain the required BIN file, but make sure the BIT file was actually generated with the multiboot features.

Invoke BIN2PHX:

    bin2phx source target name issue [extra]

`source`: source BIN file

`target`: target PHX file

`name`: core display name (16 chars max.; uppercase letters, numbers, hyphens and spaces only)

`issue`: issue number (65535 max.); this is not a build or version number, it must be bumped only on every *public*, *binary* release; 1st release must be 0

`extra`: optional extra data (raw binary data, up to 48 KB, padded to a multiple of 4 KB)


## Header

A 256-byte header is inserted before the configuration bitstream, consisting of:

* `00-0F`: Core display name
* `10-11`: Issue number
* `12-FF`: Reserved

Numbers are little-endian.


## Configuration Bitstream

A typical configuration bitstream for the Phoenix is about 456 KB long. The combination of header + bitstream is padded to 460 KB.


## Extra data

There's 52 KB worth of free space on each core slot of the console SPI flash: 4 KB at the beginning, and 48 KB at the end, which can be used at core's discretion (configuration, save data, ROM). Extra data for the 48 KB region can be supplied with this command.

When installing a core, the firmware checks if the target slot is already occupied by an earlier version of the core (comparing the core display names and issue numbers). If it is not, the updater erases the free areas not occupied by the extra data to FF. If it is, the updater does not touch those area, allowing configuration and save data to persist between updates.
