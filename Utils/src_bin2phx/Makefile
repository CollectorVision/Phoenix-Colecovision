# Phoneix utility.
#
# Creates a Phoneix core file from a binary bit-stream (has been converted from
# a .bit file to .bin file).
#
# Rev 1

.PHONY: all clean install run

all: bin2phx

clean:
	rm -f bin2phx bin2phx.exe

install: all
	cp bin2phx ../
	cp bin2phx.exe ../

run: bin2phx
	./$<

bin2phx: bin2phx.c
	gcc -Wall -o $@ $<
