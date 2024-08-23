mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))

include $(dir $(mkfile_path))/common.Makefile

# ../user/lib
USER_CODE_BASE = $(dir $(mkfile_path))../user
USER_LIB_BASE = $(USER_CODE_BASE)/lib
ULIB = $(USER_LIB_BASE)/ulib.o $(USER_LIB_BASE)/usys.o $(USER_LIB_BASE)/printf.o $(USER_LIB_BASE)/umalloc.o $(USER_LIB_BASE)/tty.o $(USER_LIB_BASE)/mutex.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -T $(USER_CODE_BASE)/userspace.ld -N -e main -Ttext 0 -o $@ $^
# if env DUMP_OBJS is set, dump the object file:
	@if [ ! -z $$DUMP_OBJS ]; then \
		$(OBJDUMP) -S $< > $*.asm; \
		$(OBJDUMP) -t $< | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym; \
	fi
