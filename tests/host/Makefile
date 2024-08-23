
HOST_TESTS_EXECUTABLES = kvectortests obj_fs_tests

all: $(HOST_TESTS_EXECUTABLES)

include ../../common/common.Makefile

# This is a generic rule to build a test executable from a .c file.
# if you want to add a new test, just add it to the list of tests, and make sure it's
# binary and *.c file have the same name, and both end with `test`.
%tests: %tests.c common_mocks.o
	$(CC) $(HOST_TESTS_CFLAGS) $(OFLAGS) -o $@ $^

common_mocks.o: common_mocks.c common_mocks.h
	$(CC) $(HOST_TESTS_CFLAGS) $(OFLAGS) -c -o $@ $<

clean:
	rm -f $(HOST_TESTS_EXECUTABLES)
	rm -rf *.o *.d *.asm *.sym
