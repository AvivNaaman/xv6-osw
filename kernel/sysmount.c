//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into vfs_file.c and vfs_fs.c.
//

#include "cgroup.h"
#include "defs.h"
#include "device/ide_device.h"
#include "device/loop_device.h"
#include "device/obj_device.h"
#include "mmu.h"
#include "mount.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"
#include "fs/fs.h"

static int handle_objfs_mounts(struct vfs_inode *mount_dir, struct mount *parent) {
  int res = -1;

  struct device *objdev = create_obj_device();
  if (objdev == NULL) {
    goto end;
  }

  res = mount(mount_dir, objdev, NULL, parent, OBJ_FS, NULL);
  deviceput(objdev);

end:
  return res;
}

static int handle_cgroup_mounts(struct vfs_inode *mount_dir, struct mount *parent, char* mount_path) {
  if (*(cgroup_root()->cgroup_dir_path)) {
    cprintf("cgroup filesystem already mounted\n");
    return -1;
  }

  set_cgroup_dir_path(cgroup_root(), mount_path);


  return 0;
}

static int handle_proc_mounts(struct vfs_inode *mount_dir, struct mount *parent, char* mount_path) {
  if (*procfs_root) {
    cprintf("proc filesystem already mounted\n");
    return -1;
  }

  set_procfs_dir_path(mount_path);

  return 0;
}

static int handle_bind_mounts(struct vfs_inode *mount_dir, struct mount *parent, char* bind_path) {
  int res = -1;
  struct vfs_inode *bind_to_dir = NULL;

  if ((bind_to_dir = vfs_namei(bind_path)) == 0) {
    cprintf("bad bind mount path\n");
    goto end;
  }

  res = mount(mount_dir, NULL, bind_to_dir, parent, NONE_FS, NULL);

end:
  if (bind_to_dir) {
    bind_to_dir->i_op->iput(bind_to_dir);
  }
  return res;
}

static int handle_nativefs_mounts(struct vfs_inode *mount_dir, struct mount *parent, const char* const device_path) {
  struct vfs_inode *loop_inode = NULL;
  struct device *loop_dev = NULL;
  int res = -1;

  if ((loop_inode = vfs_namei(device_path)) == 0) {
    cprintf("bad device_path\n");
    goto exit;
  }

  loop_inode->i_op->ilock(loop_inode);

  // find or create struct device* for loop device
  loop_dev = get_loop_device(loop_inode);
  if (loop_dev == NULL) {
    loop_dev = create_loop_device(loop_inode);
    if (loop_dev == NULL) {
      goto exit_unlock;
    }
  }

  res = mount(mount_dir, loop_dev, NULL, parent, NATIVE_FS, NULL);

exit_unlock:
  loop_inode->i_op->iunlockput(loop_inode);
  loop_inode = NULL;

exit:
  if (loop_inode) {
    loop_inode->i_op->iput(loop_inode);
  }
  if (loop_dev) {
    deviceput(loop_dev);
  }
  return res;
}

static int handle_unionfs_mounts(struct vfs_inode *mount_dir, struct mount *parent, const char* const options) {
  return mount(mount_dir, NULL, NULL, parent, UNION_FS, options);
}


// mount(options, dest_path, fstype)
int sys_mount(void) {
  char *options, *dest_path, *fstype;
  struct vfs_inode *dest_path_node = NULL;
  struct mount *parent = NULL;
  int res = -1;

  if (argstr(2, &fstype) < 0 || argstr(1, &dest_path) < 0 ||
      argstr(0, &options) < 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();
  // get dest path node & parent mount
  if ((dest_path_node = vfs_nameimount(dest_path, &parent)) == 0) {
    cprintf("bad mount dest directory\n");
    goto fail;
  }
  // make sure it is not / and it is a directory
  if (dest_path_node->type != T_DIR) {
    cprintf("mount dest not a directory\n");
    goto fail;
  }
  if (dest_path_node->inum == ROOTINO) {
    cprintf("Can't mount root directory\n");
    goto fail;
  }

  dest_path_node->i_op->ilock(dest_path_node);

  // Mount objfs file system
  if (strcmp(fstype, "objfs") == 0) {
    res = handle_objfs_mounts(dest_path_node, parent);
  } else if (strcmp(fstype, "cgroup") == 0) {
    res = handle_cgroup_mounts(dest_path_node, parent, dest_path);
  } else if (strcmp(fstype, "proc") == 0) {
    res = handle_proc_mounts(dest_path_node, parent, dest_path);
  } else if (strcmp(fstype, "bind") == 0) {
    res = handle_bind_mounts(dest_path_node, parent, options);
  } else if (strcmp(fstype, "union") == 0) {
    res = handle_unionfs_mounts(dest_path_node, parent, options);
  } else {
    res = handle_nativefs_mounts(dest_path_node, parent, options);
  } 

  // If mount failed, decrease ref.
  if (res != 0) {
    dest_path_node->i_op->iunlockput(dest_path_node);
  } else {
    dest_path_node->i_op->iunlock(dest_path_node);
  }
  dest_path_node = NULL;

fail:
  if (dest_path_node) {
    dest_path_node->i_op->iput(dest_path_node);
  }
  if (parent) {
    mntput(parent);
  }
  end_op();
  return res;
}


int sys_umount(void) {
  char *mount_path;
  int res = -1;

  if (argstr(0, &mount_path) < 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();

  // Try to umount() cgroup.
  int delete_cgroup_res = cgroup_delete(mount_path, "umount");
  if (delete_cgroup_res == RESULT_SUCCESS) {
    res = 0;
    goto end;
  }
  else if (delete_cgroup_res != RESULT_ERROR_ARGUMENT) {
    cprintf("cannot unmount cgroup\n");
    goto end;
  }


  // not cgroup, try to umount() as a regular filesystem.
  struct vfs_inode *mount_dir = NULL;
  struct mount *mnt;

  if ((mount_dir = vfs_nameimount(mount_path, &mnt)) == 0) {
    goto end;
  }
      // Make sure we are umounting a mountpoint, not just any dir.
    struct vfs_inode *mount_root_dir = get_mount_root_ip(mnt);
    if (mount_root_dir != mount_dir) {
      mount_root_dir->i_op->iput(mount_root_dir);
      cprintf("directory is not a mountpoint.\n");
x      end_op();
      return -1;
    }

    mount_root_dir->i_op->iput(mount_root_dir);
  mount_dir->i_op->iput(mount_dir);

  res = umount(mnt);
  if (res != 0) {
    mntput(mnt);
  }


end:
  end_op();
  return res;
}

int sys_pivot_root(void) {
  char *new_root = NULL;
  char *put_old = NULL;
  struct vfs_inode *new_root_inode = NULL;
  struct vfs_inode *put_old_root_inode = NULL;
  struct mount *new_root_mount = NULL;
  struct mount *put_old_root_mount = NULL;
  int res = -1;

  if (argstr(0, &new_root) < 0) {
    cprintf("badargs - new root\n");
    return 1;
  }

  if (argstr(1, &put_old) < 0) {
    cprintf("badargs - old root\n");
    return 1;
  }

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

  res = pivot_root(new_root_inode, new_root_mount, put_old_root_inode,
                   put_old_root_mount);

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
  return res;
}
