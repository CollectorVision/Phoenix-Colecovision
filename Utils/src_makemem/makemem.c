/**
 * Utility to convert a binary ROM file into a format suitable to use with
 * the Xilinx data2mem tool for merging into a bit-stream, or to generate
 * VHDL BlockRAM initialization syntax to copy-n-paste into a source file.
 *
 * Matthew Hagerty, June 2019, Public Domain.
 *
 * Unix/MinGW:
 * gcc -Wall -o makemem makemem.c
 *
 * Vistual Studio:
 * cl /W4 makemem.c
 */
#include <stdio.h>              // printf
#include <stdlib.h>             // fopen, fclose, fgetc, fputc

int
main(int argc, char *argv[])
{
    if ( argc != 4 )
    {
        printf(
        "makemem by Matthew Hagerty, version 1.0\n\n"
        "Convert a binary ROM file into a data2mem or VHDL code format.\n\n"
        "Use: makemem <format> <input rom file> <output mem file>\n\n"
        "format options:\n"
        "  m  - MEM format for data2mem\n"
        "  v  - VHDL format to include in source code for BRAM initialization\n"
        "\n"
        );

        return 1;
    }

    char type = argv[1][0];

    FILE *src = fopen(argv[2], "rb");
    FILE *dst = fopen(argv[3], "wb");

    if ( src == NULL ) {
        printf("Error, could not open input file: [%s]\n", argv[2]);
        goto DONE;
    }

    if ( dst == NULL ) {
        printf("Error, could not open or create output file: [%s]\n", argv[3]);
        goto DONE;
    }

    int c;
    int cnt = 0;
    char b2h[] = {"0123456789ABCDEF"};

    while ( (c = fgetc(src)) != EOF )
    {
        if ( type == 'v' ) {
            fprintf(dst, "x\"%c%c\",", b2h[(c>>4)], b2h[(c&0xF)]);
        }

        else
        {
            if ( (cnt % 2048) == 0 ) {
                fprintf(dst, "@%04X\n", cnt);
            }

            fprintf(dst, "%c%c ", b2h[(c>>4)], b2h[(c&0xF)]);
        }

        cnt++;
        if ( (cnt % 16) == 0 ) {
            fputc('\n', dst);
        }
    }

    DONE:

    if ( src != NULL ) {
        fclose(src);
    }

    if ( dst != NULL ) {
        fclose(dst);
    }

    return 0;
}
// main()
