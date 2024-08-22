
.PHONY: all clean

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


all: $(UPROGS)

include ../common/common.user.Makefile

clean:
	rm -f $(UPROGS) $(ULIB)
	rm -rf *.o *.d *.asm *.sym
