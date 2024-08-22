#include "defs.h"
#include "file.h"
#include "../common/fs.h"
#include "mmu.h"
#include "mount.h"
#include "../common/param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "../common/stat.h"
#include "../common/types.h"

int sys_unshare(void) {
  int nstype;
  if (argint(0, &nstype) < 0) {
    return -1;
  }

  return unshare(nstype);
}
