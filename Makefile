MAKEFILE_DIRECTORY := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

OBJS = \
	kvector.o\
	bio.o\
	console.o\
	device.o\
	exec.o\
	vfs_file.o\
	fs.o\
	vfs_fs.o\
	ide.o\
	ioapic.o\
	kalloc.o\
	kbd.o\
	lapic.o\
	log.o\
	main.o\
	kmount.o\
	mount_ns.o\
	pid_ns.o\
	mp.o\
	namespace.o\
	picirq.o\
	pipe.o\
	procfs.o\
	proc.o\
	sleeplock.o\
	spinlock.o\
	string.o\
	swtch.o\
	syscall.o\
	sysfile.o\
	sysmount.o\
	sysnamespace.o\
	sysproc.o\
	trapasm.o\
	trap.o\
	uart.o\
	vectors.o\
	vm.o\
	udiv.o\
	steady_clock.o\
	klib.o\
	cgfs.o\
	cgroup.o\
	cpu_account.o\
	obj_disk.o\
	obj_cache.o\
	obj_log.o\
	obj_fs.o



# Cross-compiling (e.g., on Mac OS X)
# TOOLPREFIX = i386-jos-elf

# Using native tools (e.g., on X86 Linux)
# TOOLPREFIX =

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your TOOLPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# If the makefile can't find QEMU, specify its path here
# QEMU = qemu-system-i386

# Try to infer the correct QEMU
ifndef QEMU
QEMU = $(shell if which qemu > /dev/null 2>&1; \
	then echo qemu; exit; \
	elif which qemu-system-i386 > /dev/null 2>&1; \
	then echo qemu-system-i386; exit; \
	elif which qemu-system-x86_64 > /dev/null 2>&1; \
	then echo qemu-system-x86_64; exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in Makefile?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

########## CFLAGS ##########
CFLAGS = -static -MD -m32 -mno-sse -gstabs -std=gnu99 -Wall -Werror -Wstack-usage=4096 \
	-fno-pic -fno-builtin -fno-strict-aliasing -fno-omit-frame-pointer $(OFLAGS) \
	-I$(MAKEFILE_DIRECTORY) -I$(MAKEFILE_DIRECTORY)/tests/framework
HOST_TESTS_CFLAGS = -static -m32 -MD -std=gnu99 -Wall -Werror -Wno-builtin-declaration-mismatch -I$(MAKEFILE_DIRECTORY) \
	-I$(MAKEFILE_DIRECTORY)/tests/host/ -I$(MAKEFILE_DIRECTORY)/tests/framework/ -DHOST_TESTS

#x86
HOST_CPU_TSC_FREQ := $(shell cat /proc/cpuinfo | grep -i "cpu mhz" | head -n 1 | rev | cut -d ' ' -f 1 | rev | cut -d '.' -f 1)*1000
#ARM
#HOST_CPU_TSC_FREQ := $(shell cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq )
CFLAGS += -DXV6_TSC_FREQUENCY=$(HOST_CPU_TSC_FREQ)

ifeq ($(pause_debug), true)
CFLAGS += -DXV6_WAIT_FOR_DEBUGGER=1
else
CFLAGS += -DXV6_WAIT_FOR_DEBUGGER=0
endif

OFLAGS = -O2
CFLAGS += -DSTORAGE_DEVICE_SIZE=327680
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
############################

ASFLAGS = -m32 -gdwarf-2 -Wa,-divide
# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1)

xv6.img: bootblock kernel fs.img | windows_debugging
	dd if=/dev/zero of=xv6.img count=10000
	dd if=bootblock of=xv6.img conv=notrunc
	dd if=kernel of=xv6.img seek=1 conv=notrunc

xv6memfs.img: bootblock kernelmemfs
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=bootblock of=xv6memfs.img conv=notrunc
	dd if=kernelmemfs of=xv6memfs.img seek=1 conv=notrunc

bootblock: bootasm.S bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
	$(OBJDUMP) -S bootblock.o > bootblock.asm
	$(OBJCOPY) -S -O binary -j .text bootblock.o bootblock
	./sign.pl bootblock

entryother: entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c entryother.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o bootblockother.o entryother.o
	$(OBJCOPY) -S -O binary -j .text bootblockother.o entryother
	$(OBJDUMP) -S bootblockother.o > entryother.asm

initcode: initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c initcode.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode.out initcode.o
	$(OBJCOPY) -S -O binary -j .text initcode.out initcode
	$(OBJDUMP) -S initcode.o > initcode.asm

kernel: entry.o entryother initcode kernel.ld $(OBJS)
	$(LD) $(LDFLAGS) -T kernel.ld -o kernel entry.o $(OBJS) -b binary initcode entryother
	$(OBJDUMP) -S kernel > kernel.asm
	$(OBJDUMP) -t kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernel.sym

# kernelmemfs is a copy of kernel that maintains the
# disk image in memory instead of writing to a disk.
# This is not so useful for testing persistent storage or
# exploring disk buffering implementations, but it is
# great for testing the kernel on real hardware without
# needing a scratch disk.
MEMFSOBJS = $(filter-out ide.o,$(OBJS)) memide.o
kernelmemfs: $(MEMFSOBJS) entry.o entryother initcode kernel.ld fs.img
	$(LD) $(LDFLAGS) -T kernel.ld -o kernelmemfs entry.o  $(MEMFSOBJS) -b binary initcode entryother fs.img
	$(OBJDUMP) -S kernelmemfs > kernelmemfs.asm
	$(OBJDUMP) -t kernelmemfs | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernelmemfs.sym

tags: $(OBJS) entryother.S _init
	etags *.S *.c

vectors.S: vectors.pl
	perl vectors.pl > vectors.S

ULIB = ulib.o usys.o printf.o umalloc.o tty.o mutex.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -T userspace.ld -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym


_forktest: forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > forktest.asm

mkfs: mkfs.c fs.h
	gcc -ggdb -Werror -Wall -o mkfs mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS_TESTS=\
	tests/xv6/_forktest\
	tests/xv6/_mounttest\
	tests/xv6/_usertests\
	tests/xv6/_pidns_tests\
	tests/xv6/_cgroupstests\
	tests/xv6/_ioctltests\

UPROGS=\
	_cat\
	_cp\
	_echo\
	_grep\
	_init\
	_kill\
	_ln\
	_ls\
	_mkdir\
	_rm\
	_sh\
	_stressfs\
	_wc\
	_zombie\
	_mount\
	_umount\
	_timer\
	_cpu\
	_mutex\
    _pouch\
    _ctrl_grp\
    _demo_pid_ns\
    _demo_mount_ns

UPROGS += $(UPROGS_TESTS)

TEST_ASSETS=

# Add test pouchfiles to the list of test assets, if the TEST_POUCHFILES env is set to 1
ifeq ($(TEST_POUCHFILES), 1)
	TEST_ASSETS += $(wildcard tests/pouchfiles/*)
endif

INTERNAL_DEV=\
	internal_fs_a \
	internal_fs_b \
	internal_fs_c

# Docker build & skopeo copy, create OCI images.
# Docker daemon must be running and available from this context.
images/img_internal_fs_%: images/build/img_internal_fs_%.Dockerfile
	docker build -t xv6_internal_fs_$* -f images/build/img_internal_fs_$*.Dockerfile images/build
	mkdir -p images/img_internal_fs_$*
	docker run --rm --mount type=bind,source="$(CURDIR)",target=/home/$(shell whoami)/xv6 \
		-w /home/$(shell whoami)/xv6 \
		-v /var/run/docker.sock:/var/run/docker.sock \
		quay.io/skopeo/stable:latest \
		copy docker-daemon:xv6_internal_fs_$*:latest oci:images/img_internal_fs_$*

# This is a dummy target to rebuild the OCI images for the internal fs.
# You should run this target if you have made changes to the internal fs build.
OCI_IMAGES = $(patsubst %, images/img_%, $(INTERNAL_DEV))
build_oci: $(OCI_IMAGES)

# internal_fs_%_img is a direcotry with the relevant OCI image to use for the internal fs build.
internal_fs_%: mkfs
	mkdir -p $(CURDIR)/images/metadata
	./images/oci_image_extractor.sh $(CURDIR)/images/extracted/$@ $(CURDIR)/images/img_$@
	echo $@ >> $(CURDIR)/images/metadata/all_images
	cd $(CURDIR)/images/extracted/$@ && find . -type f -exec ls -la {} \; > $(CURDIR)/images/metadata/img_$*.attr
	./mkfs $@ 1 $$(find $(CURDIR)/images/extracted/$@ -type f) $(CURDIR)/images/metadata/img_$*.attr
	

fs.img: mkfs README $(INTERNAL_DEV) $(UPROGS) _pouch # $(UPROGS)
	./mkfs fs.img 0 README $(UPROGS) $(INTERNAL_DEV) $(TEST_ASSETS) $(CURDIR)/images/metadata/all_images

-include *.d

#clean:
#	rm -rf mkfs bootblock.o fs.img xv6.img pouch.asm pouch.d pouch.o pouch.sym

clean: windows_debugging_clean
	rm -rf *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*.o *.d *.asm *.sym vectors.S bootblock entryother \
	initcode initcode.out kernel xv6.img fs.img kernelmemfs mkfs \
	.gdbinit objfs_tests kvector_tests \
	tests/host/*.o tests/xv6/*.o tests/xv6/*.d tests/xv6/*.asm tests/xv6/*.sym \
	$(UPROGS) \
	$(INTERNAL_DEV) \
	images/metadata images/extracted

clean_oci:
	rm -rf images/img_internal_fs_*
	docker rmi -f $(shell docker images -q -f "reference=xv6_internal_fs_*") > /dev/null 2>&1 || true


# make a printout
FILES = $(shell grep -v '^\#' runoff.list)
PRINT = runoff.list runoff.spec README toc.hdr toc.ftr $(FILES)

xv6.pdf: $(PRINT)
	./runoff
	ls -l xv6.pdf

print: xv6.pdf

# run in emulators

bochs : fs.img xv6.img
	if [ ! -e .bochsrc ]; then ln -s dot-bochsrc .bochsrc; fi
	bochs -q

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := cpus=2,cores=1
endif
QEMUOPTS = -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp $(CPUS) -m 512 $(QEMUEXTRA) -nographic

gdb: OFLAGS = -Og -ggdb
gdb: fs.img xv6.img

qemu: fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	$(QEMU) -drive file=xv6memfs.img,index=0,media=disk,format=raw -smp $(CPUS) -m 256

qemu-nox: fs.img xv6.img
	$(QEMU) -nographic $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: gdb .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -serial mon:stdio $(QEMUOPTS) -S $(QEMUGDB)

qemu-nox-gdb: gdb .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUGDB)

# CUT HERE
# prepare dist for students
# after running make dist, probably want to
# rename it to rev0 or rev1 or so on and then
# check in that version.

EXTRA=\
	mkfs.c ulib.c user.h cat.c cp.c echo.c grep.c kill.c ln.c ls.c mkdir.c rm.c\
	stressfs.c wc.c zombie.c printf.c umalloc.c mount.c umount.c timer.c cpu.c\
	mutex.c tests/xv6/forktest.c tests/xv6/mounttest.c tests/xv6/usertests.c\
	tests/xv6/pidns_tests.c tests/xv6/cgroupstests.c tests/xv6/ioctltests.c\
	README dot-bochsrc *.pl toc.* runoff runoff1 runoff.list\
	.gdbinit.tmpl gdbutil\

dist:
	rm -rf dist
	mkdir dist
	for i in $(FILES); \
	do \
		grep -v PAGEBREAK $$i >dist/$$i; \
	done
	sed '/CUT HERE/,$$d' Makefile >dist/Makefile
	echo >dist/runoff.spec
	cp $(EXTRA) dist

dist-test:
	rm -rf dist
	make dist
	rm -rf dist-test
	mkdir dist-test
	cp dist/* dist-test
	cd dist-test; $(MAKE) print
	cd dist-test; $(MAKE) bochs || true
	cd dist-test; $(MAKE) qemu

# update this rule (change rev#) when it is time to
# make a new revision.
tar:
	rm -rf /tmp/xv6
	mkdir -p /tmp/xv6
	cp dist/* dist/.gdbinit.tmpl /tmp/xv6
	(cd /tmp; tar cf - xv6) | gzip >xv6-rev10.tar.gz  # the next one will be 10 (9/17)

windows_debugging_mkdir:
	@mkdir -p windows-debugging

windows_debugging: \
	$(patsubst windows-debugging-templates/%, windows-debugging/%, $(shell find "windows-debugging-templates" -type f))

windows-debugging/%: windows-debugging-templates/% | windows_debugging_mkdir
	@rm -f $@ && \
	cp $< windows-debugging && \
	sed -i 's@{{project_root}}@$(MAKEFILE_DIRECTORY)@g' $@

windows_debugging_clean:
	@rm -rf windows-debugging

.PHONY: dist-test dist windows_debugging windows_debugging_mkdir windows_debugging_clean

# Object file system related files
# TODO integrate with the rest of xv6 sources - would be done in later part.
objfs_tests: string.c obj_disk.c obj_cache.c obj_log.c kvector.c tests/host/common_mocks.c tests/host/obj_fs_tests.c
	$(CC) $(HOST_TESTS_CFLAGS) $(OFLAGS) \
		obj_disk.c obj_cache.c obj_log.c kvector.c tests/host/common_mocks.c tests/host/obj_fs_tests.c \
		-std=gnu99 \
		-include tests/host/common_mocks.h \
		-o objfs_tests

kvector_tests: string.c kvector.c tests/host/kvectortest.c tests/host/common_mocks.c
	$(CC) $(HOST_TESTS_CFLAGS) $(OFLAGS) \
		kvector.c tests/host/kvectortest.c tests/host/common_mocks.c \
		-std=gnu99 \
		-o kvector_tests

host-tests: kvector_tests objfs_tests

host-tests-debug: OFLAGS = -Og -ggdb
host-tests-debug: host-tests
