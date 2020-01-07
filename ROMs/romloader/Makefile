# Phoenix ROM Loader
# May 2019
#
# Running on Windows via MSYS/MinGW
#
# These commands are assumed to be in the path:
#
#   sdcc, sdasz80, makebin  : part of SDCC
#   romgen                  : a MikeJ tool
#   data2mem, impact        : part of the Xilinx ISE suite
#

CP		:= cp
RM		:= rm -f
CC		:= sdcc
AS		:= sdasz80
LD		:= sdcc
#H2B		:= makebin
#ROMGEN	:= romgen
ROMGEN	:= makemem

# 24K Loader from 0000h to 5FFFh
MEMSIZE	:= 24576

BMMDIR  := ../..
BITDIR	:= ../../../collectorvision_ise
SDIR	:= src
IDIR	:= src
ODIR	:= obj

# Sadly including the peephole optimization options allows bugs to happen.
# Also, most of the other options seem to do absolutely nothing.
#CFLAGS  = -mz80 --peep-file src/peep.txt --opt-code-speed --max-allocs-per-node 200000 --allow-unsafe-read --fomit-frame-pointer -I$(IDIR) -I..
CFLAGS  = -mz80 --opt-code-speed --fomit-frame-pointer -I$(IDIR) -I..
LDFLAGS = -mz80 --code-loc 0x0100 --data-loc 0x6000 --no-std-crt0
#LDFLAGS = -mz80 --code-loc 0x1000 --data-loc 0x6000 --no-std-crt0
#LDFLAGS = -mz80 --code-loc 0x2000 --data-loc 0x6000 --no-std-crt0
#LDFLAGS = -mz80 --code-loc 0x3000 --data-loc 0x6000 --no-std-crt0
#LDFLAGS = -mz80 --code-loc 0x4000 --data-loc 0x6000 --no-std-crt0

_OBJS   = crt0.rel main.rel
OBJS    = $(patsubst %,$(ODIR)/%,$(_OBJS))

.ONESHELL:

.PHONY: all clean romloader merge load

all: romloader

clean:
	$(RM) $(ODIR)/* *.map *.lk *.noi *.bin *.ihx *.mem *.rom *.vhd *.log

#romloader: romloader.mem romloader.vhd
romloader: romloader.mem

# Update impact_load.cmd to match any bitstream file name changes, or figure
# out how to pass the bit stream file to the command file.
load: phoenix_top.merged.bit
	impact -batch impact_load.cmd

merge: phoenix_top.merged.bit

phoenix_top.merged.bit: romloader.mem
	data2mem \
		-bm $(BMMDIR)/romloader_bd.bmm \
		-bt $(BITDIR)/phoenix_top.bit \
		-bd $< tag phoenix.loader \
		-o b $(BMMDIR)/$@

# Create a VHDL inferred memory source file from the binary ROM
romloader.vhd: romloader.rom
	$(ROMGEN) $< romloader a r > $@

# Create a Xilinx format MEM file from the binary ROM
#romloader.mem: romloader.rom
#	$(ROMGEN) $< romloader m > $@
romloader.mem: romloader.rom
	$(ROMGEN) m $< $@

romloader.rom: romloader.ihx

# Make a binary file from the IHX output, padded to MEMSIZE
#%.rom: %.ihx
#	$(H2B) -s $(MEMSIZE) $< $@

# Link object files to fixed location binary
romloader.ihx: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Z80 assembly to rel (object file)
$(ODIR)/%.rel: $(SDIR)/%.s
	$(AS) -o $@ $<

# C to rel (object file)
$(ODIR)/%.rel: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
