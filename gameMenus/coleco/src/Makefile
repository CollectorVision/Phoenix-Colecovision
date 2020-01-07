# install SDCC 3.5.0 64-bit for Windows, and then in C:\program files\sdcc\bin,
# replace sdldz80.exe with Tursi's version. use gnuwin32 or cygwin for Make, and
# provide Tursi's "makeMegaCart.exe" for the linking. Alternately you can use your
# own means of linking a 24k binary, no banking. None of the special features of
# makemegacart are used. You can also skip over parseMap, that just sorts the map file.
CFLAGS = -mz80 -c "-I../libti99coleco" --allow-unsafe-read --opt-code-size --max-allocs-per-node 18000 --std-sdcc99 --vc

CC = "c:/program files/sdcc/bin/sdcc"
AS = "c:/program files/sdcc/bin/sdasz80"
AR = "c:/program files/sdcc/bin/sdcclib"
AFLAGS = -plosgff
RM = del /F 
# might need to use o for older SDCC, rel for newer
EXT=rel

# make sure the crt0 is first!
# thinfont removed for now as we can't patch the original bios
# we need it when we can as we want to allow the user to select
# fat or thin when they start, and it will remember the last
# one after reset.
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
all: bios fatfs
	"../tursi_tools/makemegacart.exe" -bios phoenixBoot.ihx phoenixBoot.rom
	"../tursi_tools/parseMap.exe" phoenixBoot.map

bios: $(MAIN_OBJS) fatfs f18a.h memset.h phoenix.h
	$(RM) phoenixBoot.ihx
	$(CC) -mz80 --no-std-crt0 --code-loc 0x0090 --data-loc 0x6400 -l../libti99coleco/libti99.a -lfatfs.a $(MAIN_OBJS) -o phoenixBoot.ihx

fatfs: $(FAT_OBJS) fatFs/ff.h fatFs/ffconf.h fatFs/diskio.h
	$(AR) -r fatfs.a $(FAT_OBJS)

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
