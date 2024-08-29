
include ../../user/common.user.Makefile

_pouch: pouch.o image.o container.o util.o configs.o $(ULIB)
	$(LD) $(LDFLAGS) -T $(dir $(mkfile_path))userspace.ld -N -e main -Ttext 0 -o $@ $^

clean:
	rm -f $(ULIB) _*
	rm -rf *.o *.d *.asm *.sym