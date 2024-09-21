#ifndef XV6_VFS_FS_H
#define XV6_VFS_FS_H

#include "stat.h"
#include "types.h"
#include "spinlock.h"

struct vfs_superblock;

struct sb_ops {
  struct vfs_inode *(*alloc_inode)(struct vfs_superblock *sb, file_type type);
  struct vfs_inode *(*get_inode)(struct vfs_superblock *sb, uint inum);
  void (*start)(struct vfs_superblock *sb);
  void (*destroy)(struct vfs_superblock *sb);
};

struct vfs_superblock {
  int ref;
  struct spinlock lock;
  void *private;
  const struct sb_ops *ops;
  struct vfs_inode *root_ip;
};

static inline void *sb_private(struct vfs_superblock *sb) {
  return sb->private;
}

// On-disk inode structure
struct vfs_dinode {
  short type;   // File type
  short major;  // Major device number (T_DEV only)
  short minor;  // Minor device number (T_DEV only)
  short nlink;  // Number of links to inode in file system
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + (sb).inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + (sb).bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

#define offsetof(TYPE, MEMBER) \
  ((unsigned int)(&((TYPE *)0)->MEMBER))  // NOLINT(runtime/casting)

#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#endif /* XV6_VFS_FS_H */
