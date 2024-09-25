#include "device.h"
#include "fs/vfs_file.h"

struct device* create_loop_device(struct vfs_inode*);
struct device* get_loop_device(const struct vfs_inode*);

struct device* create_obj_device();

struct device* create_ide_device(uint ide_port);
struct device* get_ide_device(uint ide_port);
