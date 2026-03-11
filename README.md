# minute_minute

doot dooo do do doot

boot1 replacement based on minute, includes stuff like DRAM init and PRSH handling.

![minute.png](minute.png)
![dump.png](dump.png)



## Autobooting

Autobooting can be configured in the `[boot]` section in [minute/minute.ini](config_example/minute.ini). Set `autoboot` to the number (starting at 1) of the menu entry you want to autoboot. 0 disables autobooting. A timeout in seconds can be set with the `autoboot_timeout` option (default is 3).

If no SD card is inserted, minute was loaded from SLC and the `slc:/sys/hax/ios_plugins` directory exists minute will try autobooting from SLC (first option in minute).

When a IOSU reload happens, minute will automatically boot the last booted option again, to disable this behavior set `autoreload=false` in the `[boot]` section.

## Building

To build you need [devkitPPC](https://devkitpro.org/wiki/Getting_Started) installed.

- Build `boot1.img`: `make -f Makefile.boot1`
- Build `fw.img`: `make`
- Build `fw_fastboot.img`: `make -f Makefile.fastboot`
- Build `isfshax_stage2.elf`: `make -f Makefile.isfshax`

### Building using Docker

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```bash
# Build docker image (only needed once)
docker build . -t minute-builder

# Build everything
docker run --rm -v ${PWD}:/project minute-builder /bin/bash -c "make -f Makefile.boot1 && make && make -f Makefile.fastboot && make -f Makefile.isfshax && cat boot1.img fw.img > sdcard.img"

# Clean (Note: only cleans the default build and elfloader)
docker run --rm -v ${PWD}:/project minute-builder make clean
```

## redNAND

redNAND allows replacing one or more of the Wii Us internal storage devices (SLCCMPT, SLC, MLC) with partitions on the SD card. redNAND is implemented in stroopwafel, but configured through minute. The SLC and SLCCMPT partition are without the ECC/HMAC data. \
To prepare an SD card for usage with minute you can either use the `Format redNAND` option in the `Backup and Restore` menu or partition the SD card manually on the PC.

### Format redNAND

The `Format redNAND` option will erase the FAT32 partition and recreate it smaller to make room for the redNAND partitions. If it is already small enough it won't be touched. All other partitions will get deleted and the three redNAND partitions will be created and the content of the real devices will be cloned to them.

### Partition manually

You don't need all three partitions, only the ones you intend to redirect. The first partition in the MBR (not necessarily the physical order) needs to be the FAT32 partition. The order of the redNAND partitions doesn't matter, as long as they are not the first one. Only primary partitions are supported. The purpose of the partition is identified by it's ID (file system type).
The types are:

- SLCCMPT: `0x0D`
- SLC: `0x0E` (FAT16 with LBA)
- MLC (needs SCFM): `0x83` (Linux ext2/ext3/ext4)
- MLC (no SCFM): `0x07` (NTFS)

Windows Disk Mangement doesn't support multiple partitions on SD cards, so you need to use a third party tool like Minitool Partition Wizard. The SLC partitions need to be exactly 512MiB (1048576 Sectors). If you want to write a SLC.RAW image form minute or and slc.bin from the nanddumper to it, you first need to strip the ECC data from it. If you want to use an exiting MLC image of a 32GB console, the MLC Partition needs to be exactly 60948480 sectors.

### SCFM

SCFM is a block level write cache for the MLC which resides on the SLC. This creates a coupling between to SLC and the MLC, which needs to be consistent at all times. You can not restore one without the other. This also means using the red MLC with the sys SLC or the other way around is not allowed unless explicitly enabled to prevent damage to the sys nand. \
MLC only redirection is still possible by disabling SCFM. But that requires a MLC, which is consistent without SCFM. There are two ways to archive that. Either [rebuid a fresh MLC](https://gbatemp.net/threads/how-to-upgrading-rebuilding-wii-u-internal-memory-mlc.636309/) on the redNAND partition or use a MLC Dump which was obtained through the [recovery_menu](https://github.com/jan-hofmeier/recovery_menu/releases). Format the Partition to NTFS before writing the backup to change the ID to the correct type. The MLC clone obtained by the `Format redNAND` option requires SCFM, same for the mlc.bin obtained by the original nanddumper.

### OTP

If a file `redotp.bin` is found on the SD card, it will be used instead of the real otp / otp.bin (for defuse). OTP redirection requires redirection of SLC, SLCCMPT and MLC.

### SEEPROM

SEEPROM redirection works similar to the OTP redirection, it looks for `redseeprom.bin`. If IOSU changes the SEEPROM, the changes won't be written back to the file and will be lost on reboot. The disc drive key is not redirected and is still read from the real SEEPROM.

### redNAND configuration

redNAND is configured by the [sd:/minute/rednand.ini](config_example/rednand.ini) config file.
In the `partitions` section you configure which redNAND partitions should be used. You can omit partitions that you don't want to use, but minute will warn about omitted if the partition exists on the SD. \
In the `scfm` section you configure the SCFM options. `disable` will disable the SCFM, which is required for MLC only redirection. Minute will also check if the type of the MLC partition matches this setting. The `allow_sys` allows configurations that would make your sys scfm inconsistent. This option is strongly discouraged and can will lead to corruption and data loss on the sys nand if you don't know what you are doing.
It is also possible to disable the encryption for the MLC redNAND partition using the `disable_encryption` option.
The system MLC can be mounted as a USB device, to exchange data between sysNAND and redNAND.
For setting up MLC only redNAND use this guide: [How to setup redNAND (gbatemp)](https://gbatemp.net/threads/fixing-system-memory-error-160-0103-failing-emmc-without-soldering-using-rednand-with-isfshax.642268/)
