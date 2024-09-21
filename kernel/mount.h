#ifndef XV6_MOUNT_H
#define XV6_MOUNT_H

#include "types.h"

struct mount {
  /* Pointer to the parent mount, if any. */
  struct mount *parent;
  /* Pointer to the mount point in the filesystem through which the mount is
   * accessible. */
  struct vfs_inode *mountpoint;
  /* Reference count. */
  int ref;
  /* Associated mounted FS superblock. */
  struct vfs_superblock *sb;
  /* Associated inode, applicable only for bind mounts. */
  struct vfs_inode *bind;
};

#define NMOUNT (200)

#endif /* XV6_MOUNT_H */
