CP = cp
RM = rm -f
CC = sdcc
AS = sdasz80
LD = sdcc
H2B = hex2bin

SDIR = src
IDIR = inc
ODIR = obj

CFLAGS = -mz80 --opt-code-size --fomit-frame-pointer -I$(IDIR) -I..
LDFLAGS = -mz80 --code-loc 0x8100 --data-loc 0x7000 --no-std-crt0

_OBJS = crt0.rel mmc.rel pff.rel coleco.rel main.rel
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))

.ONESHELL:

all: dir multcart.ROM

multcart.ROM: multcart.ihx
	$(H2B) -e ROM $<
#	fillfile $@ 16384
	pad $@ $@.pad 16384 255
	mv -f $@.pad $@

multcart.ihx: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

.PHONY: clean dir

clean:
	$(RM) $(ODIR)/* *.map *.lk *.noi *.bin *.ihx

#dir:
#	if not exist obj mkdir obj

$(ODIR)/%.rel: $(SDIR)/%.s
	$(AS) -o $@ $<

$(ODIR)/%.rel: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
