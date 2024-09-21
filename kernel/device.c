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
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    dev->id = i++;
    dev->ref = 0;
    dev->type = DEVICE_TYPE_NONE;
    dev->ops = NULL;
    dev->private = NULL;
  }
}

static void destroy_dev_default(struct device* dev) { dev->private = NULL; }

static void destory_loop_dev(struct device* dev) {
  // backing node can be released now.
  struct vfs_inode* loop_node = (struct vfs_inode*)dev->private;
  loop_node->i_op->iput(loop_node);
  invalidateblocks(dev);
  dev->private = NULL;
}

static const struct device_ops default_device_ops = {
    .destroy = &destroy_dev_default,
};

static const struct device_ops loop_device_ops = {
    .destroy = &destory_loop_dev,
};

struct device* getorcreatedevice(struct vfs_inode* ip) {
  acquire(&dev_holder.lock);
  struct device* empty_device = NULL;
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    if (dev->ref == 0 && empty_device == NULL) {
      empty_device = dev;
    } else if (dev->private != NULL && dev->private == ip &&
               dev->type == DEVICE_TYPE_LOOP) {
      empty_device = dev;
      break;
    }
  }

  if (empty_device == NULL) {
    release(&dev_holder.lock);
    return NULL;
  }

  empty_device->ref++;
  release(&dev_holder.lock);

  if (empty_device->ref > 1) {  // not the first reference - do not initialize.
    return empty_device;
  }

  empty_device->ref = 1;
  empty_device->type = DEVICE_TYPE_LOOP;
  empty_device->private = ip->i_op->idup(ip);
  empty_device->ops = &loop_device_ops;

  empty_device->sb.dev = empty_device;
  fsinit(&empty_device->sb);
  fsstart(&empty_device->sb);

  return empty_device;
}

struct device* getorcreateobjdevice() {
  acquire(&dev_holder.lock);
  struct device* empty_device = NULL;
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    if (dev->ref == 0 && dev->type == DEVICE_TYPE_NONE &&
        empty_device == NULL) {
      empty_device = dev;
      break;
    }
  }

  if (empty_device == NULL) {
    cprintf("No available devices!");
    release(&dev_holder.lock);
    return NULL;
  }

  empty_device->ref = 1;
  empty_device->type = DEVICE_TYPE_OBJ;
  empty_device->ops = &default_device_ops;
  empty_device->private = NULL;

  empty_device->sb.dev = empty_device;

  release(&dev_holder.lock);
  /* Save a reference to the root in order to release it in umount. */
  obj_fsinit(&empty_device->sb);
  return empty_device;
}

void deviceget(struct device* dev) {
  acquire(&dev_holder.lock);
  dev->ref++;
  release(&dev_holder.lock);
}

void deviceput(struct device* d) {
  acquire(&dev_holder.lock);
  if (d->ref == 1) {
    release(&dev_holder.lock);

    // first teardown the filesystem
    struct vfs_superblock* oldsb = &d->sb;
    oldsb->ops->destroy(oldsb);

    // now we can destroy the device.
    d->ops->destroy(d);

    // remove fields
    d->type = DEVICE_TYPE_NONE;
    d->private = NULL;
    d->ops = NULL;
    memset(&d->sb, 0, sizeof(d->sb));

    acquire(&dev_holder.lock);
  }
  d->ref--;
  release(&dev_holder.lock);
}
struct vfs_inode* getinodefordevice(struct device* dev) {
  if (dev->type != DEVICE_TYPE_LOOP) {
    return 0;
  }
  if (dev->ref == 0) {
    return 0;
  }

  return (struct vfs_inode*)dev->private;
}

struct vfs_superblock* getsuperblock(struct device* d) {
  if (d->ref == 0 || d->type == DEVICE_TYPE_NONE) {
    cprintf("getsuperblock: device not found or invalid %d\n", d->id);
    return 0;
  }
  return &d->sb;
}

int doesbackdevice(struct vfs_inode* ip) {
  acquire(&dev_holder.lock);
  for (int i = 0; i < NMAXDEVS; i++) {
    if (dev_holder.devs[i].type == DEVICE_TYPE_LOOP &&
        dev_holder.devs[i].private == ip) {
      release(&dev_holder.lock);
      return 1;
    }
  }
  release(&dev_holder.lock);
  return 0;
}

struct device* getorcreateidedevice(uint ide_port) {
  acquire(&dev_holder.lock);
  struct device* empty_device = NULL;
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    if (dev->ref == 0 && dev->type == DEVICE_TYPE_NONE &&
        empty_device == NULL) {
      empty_device = dev;
    } else if (dev->private != NULL && dev->private == (void*)ide_port &&
               dev->type == DEVICE_TYPE_IDE) {
      empty_device = dev;
      break;
    }
  }

  if (empty_device == NULL) {
    cprintf("No available devices!");
    release(&dev_holder.lock);
    return NULL;
  }

  empty_device->ref++;
  release(&dev_holder.lock);

  if (empty_device->ref > 1) {  // not the first reference - do not initialize.
    return empty_device;
  }

  empty_device->type = DEVICE_TYPE_IDE;
  empty_device->private = (void*)ide_port;
  empty_device->ops = &default_device_ops;

  empty_device->sb.dev = empty_device;
  iinit(&empty_device->sb);

  return empty_device;
}
