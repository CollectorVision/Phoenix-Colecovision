# Build the Phoenix CV Core SD-Card Menu / Loader.
#
# MUST be run from a 64-bit Windows Command Prompt.  This Makefile will not work
# with MinGW.
#
# SDCC and GNU make.exe have been included, so just type: make
#
# Built with SDCC 4.0.0 64-bit for Windows
# GNU Make.exe from http://ftp.gnu.org/gnu/make/
CFLAGS = -mz80 -c "-I../libti99coleco" "-I../sdcc/include" --opt-code-size --max-allocs-per-node 18000 --std-sdcc99 --vc --fsigned-char

CC = "..\sdcc\bin\sdcc"
AS = "..\sdcc\bin\sdasz80"
AR = "..\sdcc\bin\sdar"
MB = "..\sdcc\bin\makebin"
AFLAGS = -plosgff
RM = del /F
EXT=rel

# make sure the crt0 is first!
MAIN_OBJS =\
    crt0_bios.$(EXT)  \
    phoenixBoot.$(EXT)  \
    newmap2.$(EXT)  \
    f18a.$(EXT)  \
    mattfont.$(EXT)  \
    menu.$(EXT)  \
    vdp_memcpyfast.$(EXT) \
    joykey.$(EXT)

# By building FatFS as a library with distinct files, any functions
# we dont use are automatically stripped from the output
FAT_OBJS =\
    fatFs\diskio_diskinit.$(EXT) \
    fatFs\diskio_diskioctl.$(EXT) \
    fatFs\diskio_diskread.$(EXT) \
    fatFs\diskio_diskstatus.$(EXT) \
    fatFs\diskio_diskwrite.$(EXT) \
    fatFs\diskio_dselect.$(EXT) \
    fatFs\diskio_rcvrdatablock.$(EXT) \
    fatFs\diskio_rcvrmmc.$(EXT) \
    fatFs\diskio_select.$(EXT) \
    fatFs\diskio_sendcmd.$(EXT) \
    fatFs\diskio_waitready.$(EXT) \
    fatFs\diskio_xmitdatablock.$(EXT) \
    fatFs\diskio_xmitmmc.$(EXT) \
    fatFs\ff_checkfs.$(EXT) \
    fatFs\ff_chkchr.$(EXT) \
    fatFs\ff_closedir.$(EXT) \
    fatFs\ff_clst2sect.$(EXT) \
    fatFs\ff_cmplfn.$(EXT) \
    fatFs\ff_createchain.$(EXT) \
    fatFs\ff_createname.$(EXT) \
    fatFs\ff_diralloc.$(EXT) \
    fatFs\ff_dirclear.$(EXT) \
    fatFs\ff_dirfind.$(EXT) \
    fatFs\ff_dirnext.$(EXT) \
    fatFs\ff_dirread.$(EXT) \
    fatFs\ff_dirregister.$(EXT) \
    fatFs\ff_dirsdi.$(EXT) \
    fatFs\ff_fchdir.$(EXT) \
    fatFs\ff_fchdrive.$(EXT) \
    fatFs\ff_fclose.$(EXT) \
    fatFs\ff_findvolume.$(EXT) \
    fatFs\ff_fgetfree.$(EXT) \
    fatFs\ff_fmkdir.$(EXT) \
    fatFs\ff_fmount.$(EXT) \
    fatFs\ff_followpath.$(EXT) \
    fatFs\ff_fopen.$(EXT) \
    fatFs\ff_fread.$(EXT) \
    fatFs\ff_frename.$(EXT) \
    fatFs\ff_fsync.$(EXT) \
    fatFs\ff_ftruncate.$(EXT) \
    fatFs\ff_funlink.$(EXT) \
    fatFs\ff_fwrite.$(EXT) \
    fatFs\ff_gennumname.$(EXT) \
    fatFs\ff_getfat.$(EXT) \
    fatFs\ff_getfileinfo.$(EXT) \
    fatFs\ff_getldnumber.$(EXT) \
    fatFs\ff_loadobjxdir.$(EXT) \
    fatFs\ff_ldclust.$(EXT) \
    fatFs\ff_lddword.$(EXT) \
    fatFs\ff_ldword.$(EXT) \
    fatFs\ff_lseek.$(EXT) \
    fatFs\ff_memcmp.$(EXT) \
    fatFs\ff_movewindow.$(EXT) \
    fatFs\ff_opendir.$(EXT) \
    fatFs\ff_picklfn.$(EXT) \
    fatFs\ff_putfat.$(EXT) \
    fatFs\ff_putlfn.$(EXT) \
    fatFs\ff_pututf.$(EXT) \
    fatFs\ff_readdir.$(EXT) \
    fatFs\ff_removechain.$(EXT) \
    fatFs\ff_stclust.$(EXT) \
    fatFs\ff_storexdir.$(EXT) \
    fatFs\ff_stdword.$(EXT) \
    fatFs\ff_stword.$(EXT) \
    fatFs\ff_syncfs.$(EXT) \
    fatFs\ff_syncwindow.$(EXT) \
    fatFs\ff_sumsfn.$(EXT) \
    fatFs\ff_tchar2uni.$(EXT) \
    fatFs\ff_validate.$(EXT) \
    fatFs\ff_vars.$(EXT) \
    fatFs\ff_xsum32.$(EXT) \
    fatFs\ffunicode_cp.$(EXT) \
    fatFs\ffunicode_ffoem2uni.$(EXT) \
    fatFs\ffunicode_ffuni2oem.$(EXT) \
    fatFs\ffunicode_ffwtoupper.$(EXT)

# Recipe to compile the app
.phony all: link

link: bios fatfs
	$(MB) -s 24576 phoenixBoot.ihx >phoenixBoot.rom

# tursi's special link step - requires makemegacart and parsemap tools
# makemegacart does the normal link step in this case but emits debug information
# parsemap just sorts the map file
# these tools are not included as they are not useful to the average person, but
# are available on github
tursi: bios fatfs
	"../makemegacart.exe" -bios phoenixBoot.ihx phoenixBoot.rom
	"../parseMap.exe" phoenixBoot.map

bios: $(MAIN_OBJS) fatfs f18a.h memset.h phoenix.h
	$(RM) phoenixBoot.ihx
	$(CC) -mz80 --no-std-crt0 --code-loc 0x0090 --data-loc 0x6400 -l../libti99coleco/libti99.a -lfatfs.a $(MAIN_OBJS) -o phoenixBoot.ihx

fatfs: $(FAT_OBJS) fatFs/ff.h fatFs/ffconf.h fatFs/diskio.h
	$(AR) -rc fatfs.a $(FAT_OBJS)

# Recipe to clean all compiled objects
.phony clean:
	$(RM) *.rel *.map *.lst *.lnk *.sym *.asm *~ *.o *.obj *.ihx *.rom *.rel *.a *.lib
	$(RM) fatFs\*.rel fatFs\*.map fatFs\*.lst fatFs\*.lnk fatFs\*.sym fatFs\*.asm fatFs\*~ fatFs\*.o fatFs\*.obj fatFs\*.ihx fatFs\*.rom fatFs\*.rel fatFs\*.a fatFs\*.lib

# Recipe to compile all C files
%.rel: %.c
	$(CC) -c $< $(CFLAGS) -o $@

# Recipe to compile all assembly files
%.rel: %.s
	$(AS) -o $@ $<
