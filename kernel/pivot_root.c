#include "defs.h"
#include "fs/vfs_file.h"
#include "fs/vfs_fs.h"
#include "mount.h"
#include "stat.h"

#define PIVOT_SUCCESS 0
#define PIVOT_FAILURE (-1)

static int pivot_root(char* const new_root, char* const put_old) {
  struct vfs_inode* new_root_inode = NULL;
  struct vfs_inode* put_old_root_inode = NULL;
  struct mount* new_root_mount = NULL;
  struct mount* put_old_root_mount = NULL;

  int status = PIVOT_FAILURE;
  // check that new_root is a mount point.
  // check that put_old should be a dir under new_root.

  new_root_inode = vfs_nameimount(new_root, &new_root_mount);
  if (new_root_inode == NULL) {
    cprintf("Failed to get new root dir inode\n");
    goto end;
  }

  if (new_root_inode->type != T_DIR) {
    cprintf("new root mount path is not a mountpoint\n");
    goto end;
  }

  put_old_root_inode = vfs_nameimount(put_old, &put_old_root_mount);
  if (put_old_root_inode == NULL) {
    cprintf("Failed to get old root dir inode\n");
    goto end;
  }

  if (put_old_root_inode->type != T_DIR) {
    cprintf("old root mount path is not a dir\n");
    goto end;
  }

  setrootmount(new_root_mount);
  status = PIVOT_SUCCESS;

end:
  if (new_root_inode != NULL) {
    new_root_inode->i_op->iput(new_root_inode);
  }
  if (put_old_root_inode != NULL) {
    put_old_root_inode->i_op->iput(put_old_root_inode);
  }
  if (new_root_mount != NULL) {
    mntput(new_root_mount);
  }
  if (put_old_root_mount != NULL) {
    mntput(put_old_root_mount);
  }
  return status;
}

int sys_pivot_root(void) {
  char* new_root;
  char* put_old;

  if (argstr(0, &new_root) < 0) {
    cprintf("badargs - new root\n");
    return PIVOT_FAILURE;
  }

  if (argstr(1, &put_old) < 0) {
    cprintf("badargs - old root\n");
    return PIVOT_FAILURE;
  }

  return pivot_root(new_root, put_old);
}
