include ../common/common.Makefile

ULIB = lib/ulib.o lib/usys.o lib/printf.o lib/umalloc.o lib/tty.o lib/mutex.o

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
    _pouch\
    _ctrl_grp\
    _demo_pid_ns\
    _demo_mount_ns

.PRECIOUS: %.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -T userspace.ld -N -e main -Ttext 0 -o $@ $^
	# if env DUMP_OBJS is set, dump the object file:
	@if [ ! -z $$DUMP_OBJS ]; then \
		$(OBJDUMP) -S $< > $*.asm; \
		$(OBJDUMP) -t $< | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym; \
	fi

all: $(UPROGS)

clean:
	rm -f *.o *.d *.asm *.sym $(UPROGS) $(ULIB)
