#ifndef XV6_FS_UNIONFS_H
#define XV6_FS_UNIONFS_H

#include "types.h"
#include "param.h"
#include "vfs_fs.h"
#include "vfs_file.h"

#define UNION_FS_MAX_LAYERS 2
#define UNION_FS_OPTIONS_SEP ';'

#define UNION_FS_TOP_LAYER_INDEX 0

void union_iinit();
int unionfs_init(struct vfs_superblock*, const char* options);

struct unionfs_layer_info {
    struct vfs_inode* inode;
};

struct unionfs_parsed_options {
    struct unionfs_layer_info layers_inodes[UNION_FS_MAX_LAYERS];
    int nlayers;
};

struct unionfs_superblock_private {
    struct unionfs_parsed_options options;
};

struct unionfs_inode {
    struct vfs_inode vfs_inode;

    union {
        struct {
            /** The inode of the underlying filesystem for this inode. */
            struct vfs_inode* underlying_inode;
            /** What layer did this inode's underlying inode came from? */
            int layer_index;
        } file;
        struct {
            /**
             * For each layer, the inode of the directory entry in that layer.
             */
            struct vfs_inode* inodes[UNION_FS_MAX_LAYERS];
        } dir;
    } underlying;
};

#endif // XV6_FS_UNIONFS_H
