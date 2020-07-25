/**
 * @file bit2bin.c
 *
 * Borrowed from:
 * https://github.com/sadman/zxuno-mirror/blob/master/firmware/roms/Bit2Bin.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *fi, *fo;
int i, length;
unsigned char mem[0x4000];
unsigned short j;

int main(int argc, char *argv[]) {
  if( argc==1 )
    printf("\n"
    "Bit2Bin v0.02, strip .bit header and align binary to 16k, 2016-02-23\n\n"
    "  Bit2Bin        <input_file> <output_file>\n\n"
    "  <input_file>   Input BIT file\n"
    "  <output_file>  Output BIN file\n\n"
    "All params are mandatory\n\n"),
    exit(0);
  if( argc!=3 )
    printf("\nInvalid number of parameters\n"),
    exit(-1);
  fi= fopen(argv[1], "rb");
  if( !fi )
    printf("\nInput file not found: %s\n", argv[1]),
    exit(-1);
  fseek(fi, 0, SEEK_END);
  i= ftell(fi);
  fseek(fi, 0, SEEK_SET);
  fread(mem, 1, 2, fi);
  i-= (j= mem[1]|mem[0]<<8)+4;
  fread(mem, 1, j+2, fi);
  i-= (j= mem[j+1]|mem[j]<<8)+3;
  fread(mem, 1, j+3, fi);
  i-= (j= mem[j+1]|mem[j]<<8)+3;
  fread(mem, 1, j+3, fi);
  i-= (j= mem[j+1]|mem[j]<<8)+3;
  fread(mem, 1, j+3, fi);
  i-= (j= mem[j+1]|mem[j]<<8)+3;
  fread(mem, 1, j+3, fi);
  i-= (j= mem[j+1]|mem[j]<<8)+4;
  fread(mem, 1, j+4, fi);
  length= mem[j+3]|mem[j+2]<<8|mem[j+1]<<16|mem[j]<<24;
  if( i!=length )
    printf("\nInvalid file length\n"),
    exit(-1);
  fo= fopen(argv[2], "wb+");
  if( !fo )
    printf("\nCannot create output file: %s\n", argv[2]),
    exit(-1);
  j= i>>14;
  if( j )
    for ( i= 0; i<j; i++ )
      fread(mem, 1, 0x4000, fi),
      fwrite(mem, 1, 0x4000, fo);
  memset(mem, 0, 0x4000);
  fread(mem, 1, length&0x3fff, fi),
  fwrite(mem, 1, 0x4000, fo);
  memset(mem, 0, 0x4000);
  for ( i= 0; i<20-j; i++ )
    fwrite(mem, 1, 0x4000, fo);
  printf("\nFile generated successfully\n");
  fclose(fo);
}
