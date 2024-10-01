#ifndef XV6_FS_FS_H
#define XV6_FS_FS_H

#include "vfs_fs.h"

void fsinit();

typedef enum fstype {
    NONE_FS = 0,
    NATIVE_FS,
    OBJ_FS,
    PROC_FS,
    CGROUP_FS,
    UNION_FS,
} fstype;

#endif  // XV6_FS_FS_H
