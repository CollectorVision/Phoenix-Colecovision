# Build a utility to strip Xilinx bit-stream .bit header and align binary to 16k.
#
# Borrowed from:
#  https://github.com/sadman/zxuno-mirror/blob/master/firmware/roms/Bit2Bin.c
#
# Rev 1

.PHONY: all clean install

all: bit2bin

clean:
	rm -f bit2bin bit2bin.exe

install: all
	cp bit2bin ../
	cp bit2bin.exe ../

bit2bin: bit2bin.c
	gcc -Wall -o $@ $<
