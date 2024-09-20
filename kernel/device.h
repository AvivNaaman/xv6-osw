#ifndef XV6_DEVICE_H
#define XV6_DEVICE_H

#include "fs.h"
#include "obj_fs.h"
#include "spinlock.h"

#define NLOOPDEVS_ (10)
#define NIDEDEVS_ (2)
#define NOBJDEVS_ (2)

#define NMAXDEVS (NLOOPDEVS_ + NIDEDEVS_ + NOBJDEVS_)

struct device;

enum device_type {
  DEVICE_TYPE_NONE = 0,
  DEVICE_TYPE_IDE,
  DEVICE_TYPE_LOOP,
  DEVICE_TYPE_OBJ,
};

struct device_ops {
  void (*destroy)(struct device* dev);
};

struct device {
  int ref;
  int id;
  struct vfs_superblock sb;
  enum device_type type;
  void* private;
  const struct device_ops* ops;
};

struct dev_holder_s {
  struct spinlock lock;  // protects loopdevs
  struct device devs[NMAXDEVS];
};

extern struct dev_holder_s dev_holder;

#endif /* XV6_DEVICE_H */
