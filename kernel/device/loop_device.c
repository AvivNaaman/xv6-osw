#include "devices.h"

static void destory_loop_dev(struct device* const dev) {
  // backing node can be released now.
  struct vfs_inode* loop_node = (struct vfs_inode*)dev->private;
  loop_node->i_op->iput(loop_node);
  invalidateblocks(dev);
  dev->private = NULL;
}

static const struct device_ops loop_device_ops = {
    .destroy = &destory_loop_dev,
};

struct device* get_loop_device(const struct vfs_inode* const ip) {
  acquire(&dev_holder.lock);
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    if (dev->private != NULL && dev->private == ip &&
        dev->type == DEVICE_TYPE_LOOP) {
      dev->ref++;
      release(&dev_holder.lock);
      return dev;
    }
  }
  release(&dev_holder.lock);
  return NULL;
}

struct device* create_loop_device(struct vfs_inode* const ip) {
  struct device* dev = get_new_device(DEVICE_TYPE_LOOP);

  if (dev == NULL) {
    return NULL;
  }

  dev->private = ip->i_op->idup(ip);
  dev->ops = &loop_device_ops;

  return dev;
}
