CURRENT_MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))

TEST_EXECUTABLES = _forktest _ioctltests _mounttest _usertests _pidns_tests _cgroupstests

all: $(TEST_EXECUTABLES)

include $(dir $(CURRENT_MAKEFILE_PATH))../../common/common.user.Makefile

clean:
	rm -f $(TEST_EXECUTABLES) $(ULIB)
	rm -rf *.o *.d *.asm *.sym
