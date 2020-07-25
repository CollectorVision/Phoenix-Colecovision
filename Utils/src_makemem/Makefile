# Makefile to build the makemem utility.
#
# The makemem utility will convert a binary ROM file into a format suitable to
# use with the Xilinx data2mem tool for merging into a bit-stream, or to
# generate VHDL BlockRAM initialization syntax to copy-n-paste into a source
# file.
#

.PHONY: all clean install

all: makemem

clean:
	rm -f makemem makemem.exe

install: all
	cp makemem ../
	cp makemem.exe ../

makemem: makemem.c
	gcc -Wall -o $@ $<
