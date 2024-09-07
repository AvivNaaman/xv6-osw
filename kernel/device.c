#include "device.h"

#include "defs.h"
#include "file.h"
#include "obj_disk.h"
#include "sleeplock.h"
#include "types.h"

struct dev_holder_s dev_holder = {0};

void devinit(void) {
  int i = 0;
  initlock(&dev_holder.lock, "dev_list");

  // Initiate ref of obj device for future use. (if the device will be save as
  // internal_fs files and not in the ram)
  for (i = 0; i < NOBJDEVS; i++) {
    dev_holder.objdev[i].ref = 0;
  }
}

void objdevinit(uint dev) {
  if (dev < NOBJDEVS) {
    memcpy(&dev_holder.objdev[dev].sb, memory_storage,
           sizeof(dev_holder.objdev[dev].sb));
  }
}

int getorcreatedevice(struct vfs_inode *ip) {
  acquire(&dev_holder.lock);
  int emptydevice = -1;
  for (int i = 0; i < NLOOPDEVS; i++) {
    if (dev_holder.loopdevs[i].ref == 0 && emptydevice == -1) {
      emptydevice = i;
    } else if (dev_holder.loopdevs[i].loop_node == ip) {
      dev_holder.loopdevs[i].ref++;
      release(&dev_holder.lock);
      return LOOP_DEVICE_TO_DEV(i);
    }
  }

  if (emptydevice == -1) {
    release(&dev_holder.lock);
    return -1;
  }

  dev_holder.loopdevs[emptydevice].ref = 1;
  dev_holder.loopdevs[emptydevice].loop_node = ip->i_op->idup(ip);

  release(&dev_holder.lock);
  fsinit(LOOP_DEVICE_TO_DEV(emptydevice));
  fsstart(LOOP_DEVICE_TO_DEV(emptydevice));
  return LOOP_DEVICE_TO_DEV(emptydevice);
}

int getorcreateobjdevice() {
  acquire(&dev_holder.lock);
  int emptydevice = -1;
  for (int i = 0; i < NOBJDEVS; i++) {
    if (dev_holder.objdev[i].ref == 0 && emptydevice == -1) {
      emptydevice = i;
    }
  }

  if (emptydevice == -1) {
    cprintf("Not available obj device\n");
    release(&dev_holder.lock);
    return -1;
  }

  dev_holder.objdev[emptydevice].ref = 1;
  release(&dev_holder.lock);
  objdevinit(emptydevice);
  /* Save a reference to the root in order to release it in umount. */
  obj_fsinit(OBJ_TO_DEV(emptydevice));
  return OBJ_TO_DEV(emptydevice);
}

void deviceget(uint dev) {
  if (IS_LOOP_DEVICE(dev)) {
    dev = DEV_TO_LOOP_DEVICE(dev);
    acquire(&dev_holder.lock);
    dev_holder.loopdevs[dev].ref++;
    release(&dev_holder.lock);
  } else if (IS_OBJ_DEVICE(dev)) {
    dev = DEV_TO_OBJ_DEVICE(dev);
    acquire(&dev_holder.lock);
    dev_holder.objdev[dev].ref++;
    release(&dev_holder.lock);
  }
}

void deviceput(uint dev) {
  if (IS_LOOP_DEVICE(dev)) {
    dev = DEV_TO_LOOP_DEVICE(dev);
    acquire(&dev_holder.lock);
    if (dev_holder.loopdevs[dev].ref == 0) {
      panic("deviceput: device ref count is 0");
    }
    if (dev_holder.loopdevs[dev].ref == 1) {
      release(&dev_holder.lock);

      dev_holder.loopdevs[dev].loop_node->i_op->iput(
          dev_holder.loopdevs[dev].loop_node);
      invalidateblocks(LOOP_DEVICE_TO_DEV(dev));

      acquire(&dev_holder.lock);
      dev_holder.loopdevs[dev].loop_node = NULL;
    }
    dev_holder.loopdevs[dev].ref--;
    release(&dev_holder.lock);
  } else if (IS_OBJ_DEVICE(dev)) {
    dev = DEV_TO_OBJ_DEVICE(dev);
    acquire(&dev_holder.lock);
    dev_holder.objdev[dev].ref--;
    if (dev_holder.objdev[dev].ref == 1) {
      release(&dev_holder.lock);

      struct vfs_inode *root_ip = dev_holder.objdev[dev].sb.root_ip;
      root_ip->i_op->iput(root_ip);

      acquire(&dev_holder.lock);
      dev_holder.objdev[dev].sb.root_ip = NULL;
    }
    release(&dev_holder.lock);
  }
}

struct vfs_inode *getinodefordevice(uint dev) {
  if (!IS_LOOP_DEVICE(dev)) {
    return 0;
  }

  dev = DEV_TO_LOOP_DEVICE(dev);

  if (dev_holder.loopdevs[dev].ref == 0) {
    return 0;
  }

  return dev_holder.loopdevs[dev].loop_node;
}

struct vfs_superblock *getsuperblock(uint dev) {
  if (IS_LOOP_DEVICE(dev)) {
    uint loopdev = DEV_TO_LOOP_DEVICE(dev);
    if (loopdev >= NLOOPDEVS) {
      panic("could not find superblock for device: device number to high");
    }
    if (dev_holder.loopdevs[loopdev].ref == 0) {
      panic("could not find superblock for device: device ref count is 0");
    } else {
      return &dev_holder.loopdevs[loopdev].sb;
    }
  } else if (IS_OBJ_DEVICE(dev)) {
    uint objdev = DEV_TO_OBJ_DEVICE(dev);
    if (objdev >= NOBJDEVS) {
      panic("could not find obj superblock for device: device number to high");
    }
    if (dev_holder.objdev[objdev].ref == 0) {
      panic("could not find obj superblock for device: device ref count is 0");
    } else {
      return &dev_holder.objdev[objdev].sb;
    }
  } else if (dev < NIDEDEVS) {
    return &dev_holder.idesb[dev];
  } else {
    cprintf("could not find superblock for device, dev: %d\n", dev);
    panic("could not find superblock for device");
  }
}

int doesbackdevice(struct vfs_inode *ip) {
  acquire(&dev_holder.lock);
  for (int i = 0; i < NLOOPDEVS; i++) {
    if (dev_holder.loopdevs[i].loop_node == ip) {
      release(&dev_holder.lock);
      return 1;
    }
  }
  release(&dev_holder.lock);
  return 0;
}
