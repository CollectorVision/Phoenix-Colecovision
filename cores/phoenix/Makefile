# Phoenix ROM Loader
# May 2019
#
# Running on Windows via MSYS/MinGW
#
# These utilities are included:
#
#   data2mem                : part of the Xilinx ISE suite.
#   makemem					: generate a data2mem file from a ROM.
#

CP		:= cp
RM		:= rm -f
DAT2MEM := ../../Utils/data2mem
MEMGEN	:= ../../Utils/makemem
BIT2BIN := ../../Utils/bit2bin
BIN2PHX := ../../Utils/bin2phx

# Phoenix CV core revision.
COREREV	:= 8

# Source bit-stream and output merged bit-stream directory.
BMMDIR  := ../../rtl
BITDIR	:= ../../../collectorvision_ise
# Phoenix Boot ROM directory.
SDIR	:= ../../gameMenus/coleco/src
# Where to put merged bit-stream and core files.
OUTDIR  := ../..


.ONESHELL:

.PHONY: help all clean merge corerom

help:
	@echo "Known make targets: help all clean merge corerom"

all: corerom

clean:
	$(RM) *.bin *.mem *.rom *.PHX


corerom: CORE01.PHX

# Make the CORE01.PHX Phoenix CV Menu core.
CORE01.PHX: phoenix_top.merged.bin
	$(BIN2PHX) $< $(OUTDIR)/$@ COLECOVISION $(COREREV)

# Strip the bit-stream header.
phoenix_top.merged.bin: phoenix_top.merged.bit
	$(BIT2BIN) $(OUTDIR)/$< $@


# Merge the ROM with the CV bit-stream.
merge: phoenix_top.merged.bit

phoenix_top.merged.bit: phoenixBoot.mem
	$(DAT2MEM) \
		-bm $(BMMDIR)/romloader_bd.bmm \
		-bt $(BITDIR)/phoenix_top.bit \
		-bd $< tag phoenix.loader \
		-o b $(OUTDIR)/$@

# Create a Xilinx format MEM file from the binary ROM.
phoenixBoot.mem: $(SDIR)/phoenixBoot.rom
	$(MEMGEN) m $< $@
