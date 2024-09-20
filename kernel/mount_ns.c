#include "mount_ns.h"

#include "defs.h"
#include "file.h"
#include "fs.h"
#include "mmu.h"
#include "mount.h"
#include "namespace.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

struct {
  struct spinlock lock;
  struct mount_ns mount_ns[NNAMESPACE];
} mountnstable;


static struct mount_ns* allocmount_ns() {
  acquire(&mountnstable.lock);
  for (int i = 0; i < NNAMESPACE; i++) {
    if (mountnstable.mount_ns[i].ref == 0) {
      struct mount_ns* mount_ns = &mountnstable.mount_ns[i];
      mount_ns->ref = 1;
      release(&mountnstable.lock);
      return mount_ns;
    }
  }
  release(&mountnstable.lock);

  panic("out of mount_ns objects");
}

void mount_nsinit() {
  initlock(&mountnstable.lock, "mountns");
  for (int i = 0; i < NNAMESPACE; i++) {
    initlock(&mountnstable.mount_ns[i].lock, "mount_ns");
  }
  // Create the initial mount namespace
  allocmount_ns();
}

struct mount_ns* mount_nsdup(struct mount_ns* mount_ns) {
  acquire(&mountnstable.lock);
  mount_ns->ref++;
  release(&mountnstable.lock);

  return mount_ns;
}

void mount_nsput(struct mount_ns* mount_ns) {
  acquire(&mountnstable.lock);
  if (mount_ns->ref == 1) {
    release(&mountnstable.lock);

    umountall(mount_ns->active_mounts);
    mount_ns->active_mounts = 0;

    acquire(&mountnstable.lock);
  }
  mount_ns->ref--;
  release(&mountnstable.lock);
}

struct mount_ns* copymount_ns() {
  struct mount_ns* mount_ns = allocmount_ns();
  mount_ns->active_mounts = copyactivemounts();
  mount_ns->root = getroot(mount_ns->active_mounts);
  return mount_ns;
}

struct mount_ns* getinitmountns() {
  struct mount_ns* to_return = &mountnstable.mount_ns[0];
  mount_nsdup(to_return);
  return to_return;
}