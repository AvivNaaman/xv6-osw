#include "unionfs.h"
#include "defs.h"
#include "mmu.h"
#include "vfs_fs.h"
#include "vfs_file.h"

static struct inode_operations unionfs_inode_ops;

static int unionfs_parse_options(const char* option_str, struct unionfs_parsed_options * const options) {
  const char* current_options_position = option_str;
  for (options->nlayers = 0; options->nlayers < UNION_FS_MAX_LAYERS; ++options->nlayers) {
    // copy until the next separator or the end of the string, making sure we don't overflow max path length:
    char current_layer[MAX_PATH_LENGTH] = {0};
    for (char* current_path = current_layer;
         current_path < &current_layer[MAX_PATH_LENGTH];
         ++current_path, ++current_options_position) {
      if (*current_options_position == UNION_FS_OPTIONS_SEP || *current_options_position == '\0') {
        *current_path = '\0';
        break;
      }
      *current_path = *current_options_position;
    }

    // if overflow, error:
    if (*current_options_position != UNION_FS_OPTIONS_SEP && *current_options_position != '\0') {
      cprintf("unionfs: layer path too long\n");
      return -1;
    }

    // query layer fs structs
    struct vfs_inode *layer_inode = vfs_namei(current_layer);
    if (layer_inode == NULL) {
      cprintf("unionfs: failed to find inode for layer %s\n", current_layer);
      return -1;
    }
    if (layer_inode->type != T_DIR) {
      cprintf("unionfs: layer %s is not a directory\n", current_layer);
      // TODO: Error cleanup...
      return -1;
    }
    options->layers_inodes[options->nlayers].inode = layer_inode;
  }
  return 0;
}

static inline struct unionfs_superblock_private *union_sb_private(struct vfs_superblock *sb) {
  return (struct unionfs_superblock_private *)sb_private(sb);
}

static inline void union_copy_vfs_inodes(struct vfs_inode* dest, struct vfs_inode* src) {
  XV6_ASSERT(holdingsleep(&src->lock) && holdingsleep(&dest->lock));
  dest->type = src->type;
  dest->major = src->major;
  dest->minor = src->minor;
  dest->nlink = src->nlink;
  dest->size = src->size;
}

struct {
  struct spinlock lock;
  struct unionfs_inode inode[NINODE];
} union_icache;

static struct vfs_inode* union_idup(struct vfs_inode *ip) {
  acquire(&union_icache.lock);
  ip->ref++;
  release(&union_icache.lock);
  return ip;
}

/** Allocates a new unionfs_inode from the inode cache, sets ref=1 and vfs_inode.type to type. */
static struct unionfs_inode* union_ialloc_internal(struct vfs_superblock* const sb, const file_type type) {
// find a free inode in the cache:
  struct unionfs_inode *empty = NULL;
  acquire(&union_icache.lock);
  for (struct unionfs_inode *ip = &union_icache.inode[0]; ip < &union_icache.inode[NINODE]; ++ip) {
    if (ip->vfs_inode.ref == 0) {
      empty = ip;
      empty->vfs_inode.ref = 1;
      break;
    }
  }
  release(&union_icache.lock);
  if (empty == NULL) {
    cprintf("unionfs: inode cache full\n");
  }
  empty->vfs_inode.sb = sb;
  empty->vfs_inode.type = type;
  return empty;
}

/** 
 * Returns the top-level underlying inode for the current inode.
 * If this is a file, returns the underlying file inode.
 * If this is a directory, returns the first non-NULL inode in the directory inode list.
 */
static inline struct vfs_inode* top_inode(struct vfs_inode *dip) {
  struct unionfs_inode *udip = container_of(dip, struct unionfs_inode, vfs_inode);
  if (dip->type == T_FILE) {
    return udip->underlying.file.underlying_inode;
  }

  XV6_ASSERT(dip->type == T_DIR);
  struct unionfs_superblock_private *sb_private = union_sb_private(dip->sb);
  for (int i = 0; i < sb_private->options.nlayers; ++i) {
    if (udip->underlying.dir.inodes[i] != NULL) {
      return udip->underlying.dir.inodes[i];
    }
  }
  XV6_ASSERT(0 && "no underlying inode found for directory");
}

static void union_iput(struct vfs_inode *ip) {
  XV6_ASSERT(ip->ref > 0);
  struct unionfs_inode *uip = (struct unionfs_inode *)ip;
  acquire(&union_icache.lock);
  if (ip->ref != 1) {
    return;
  }
  // release from cache
  ip->ref = 0;
  release(&union_icache.lock);

  struct unionfs_superblock_private *sb_private = union_sb_private(ip->sb);

  // release underlying inodes
  if (ip->type == T_FILE) {
    struct vfs_inode *top = top_inode(ip);
    top->i_op->iput(top);
  } else if (ip->type == T_DIR) {
    for (struct vfs_inode** curr_layer_dir_inode = uip->underlying.dir.inodes;
         curr_layer_dir_inode < &uip->underlying.dir.inodes[sb_private->options.nlayers];
         ++curr_layer_dir_inode) {
      if (*curr_layer_dir_inode != NULL) {
        (*curr_layer_dir_inode)->i_op->iput(*curr_layer_dir_inode);
      }
    }
  }
}

static void union_ilock(struct vfs_inode *ip) {
  struct unionfs_inode *uip = (struct unionfs_inode *)ip;
  acquiresleep(&uip->vfs_inode.lock);
  struct vfs_inode *top = top_inode(ip);
  top->i_op->ilock(top);
  // Copy fields from underlying inode to vfs inode.
  // TODO: This is not necessarily correct for directories!
  union_copy_vfs_inodes(&uip->vfs_inode, top);
}

static void union_iunlock(struct vfs_inode *ip) {
  struct unionfs_inode *uip = (struct unionfs_inode *)ip;
  // Write-back updated fields to underlying inode
  struct vfs_inode *top = top_inode(ip);
  union_copy_vfs_inodes(top, &uip->vfs_inode);
  top->i_op->iunlock(top);
  releasesleep(&uip->vfs_inode.lock);
}

static void union_iunlockput(struct vfs_inode *ip) {
  union_iunlock(ip);
  union_iput(ip);
}

static int union_readi(struct vfs_inode *ip, uint off, uint n, vector *dstvector) {
  // This is just wrong. What about directory reading?
  if (ip->type != T_FILE) {
    panic("union_readi not file");
  }
  struct vfs_inode *top = top_inode(ip);
  return top->i_op->readi(top, off, n, dstvector);
}

static int union_isdirempty(struct vfs_inode *ip) {
  if (ip->type != T_DIR) {
    panic("union_isdirempty not dir");
  }
  struct unionfs_inode *udip = container_of(ip, struct unionfs_inode, vfs_inode);
  // Iterate layers inodes and check if they are all empty.
  for (struct vfs_inode** curr_layer_dir_inode = udip->underlying.dir.inodes;
       curr_layer_dir_inode < &udip->underlying.dir.inodes[UNION_FS_MAX_LAYERS];
       ++curr_layer_dir_inode) {
    if (*curr_layer_dir_inode != NULL) {
      if (!(*curr_layer_dir_inode)->i_op->isdirempty(*curr_layer_dir_inode)) {
        return false;
      }
    }
  }
  return true;
}

static struct vfs_inode* union_dirlookup(struct vfs_inode *dip, char *name, uint *poff) {
  // TODO: support poff != NULL.
  XV6_ASSERT(poff == NULL);
  if (dip->type != T_DIR) panic("dirlookup not DIR");

  struct unionfs_inode *udip = container_of(dip, struct unionfs_inode, vfs_inode);
  struct unionfs_superblock_private *sb_private = union_sb_private(dip->sb);
  // TODO: Lock? Try get before allocating? Update content when lock/unlock?
  // Iterate layers inodes 
  struct unionfs_inode *result = NULL;
  for (int i = 0; i < sb_private->options.nlayers; ++i) {
    struct vfs_inode *layer_inode = udip->underlying.dir.inodes[i];
    struct vfs_inode *found = NULL;
    if (layer_inode != NULL) {
      found = layer_inode->i_op->dirlookup(layer_inode, name, NULL);
      if (found != NULL) {
        return found;
      }
    } 

    if (!found) continue;
    XV6_ASSERT(found->type == T_FILE || found->type == T_DIR);

    // If we found a file before anything, set it and no more llokup needed.
    if (result == NULL && found->type == T_FILE) {
      result = union_ialloc_internal(dip->sb, found->type); // We only like to allocate the inode from our cache, not from underlying fs.
      result->underlying.file.underlying_inode = found;
      result->underlying.file.layer_index = i;
      break;
    }

    if (result == NULL) {
      // allocate dir, and set previous layers to NULL -- looking up in them was empty.
      result = union_ialloc_internal(dip->sb, found->type);
      for (int j = 0; j < i; ++j) {
        result->underlying.dir.inodes[j] = NULL;
      }
    }
    XV6_ASSERT(result->vfs_inode.type == T_DIR);
    // set the found inode in the current layer.
    result->underlying.dir.inodes[i] = found;
  }

  if (result == NULL) {
    return NULL;
  }

  return &result->vfs_inode;
}

static void union_stati(struct vfs_inode *ip, struct stat *st) {
  struct vfs_inode *top = top_inode(ip);
  return top->i_op->stati(top, st);
}


static struct vfs_inode* union_ialloc(struct vfs_superblock* const sb, const file_type type) {
  const struct unionfs_superblock_private *const sb_private = union_sb_private(sb);
  XV6_ASSERT(type == T_FILE || type == T_DIR);
  // allocate on top-layer.
  struct vfs_superblock* const alloc_on_sb = sb_private->options.layers_inodes[UNION_FS_TOP_LAYER_INDEX].inode->sb;
  struct vfs_inode * const underlying_inode = alloc_on_sb->ops->ialloc(alloc_on_sb, type);
  
  struct unionfs_inode *empty = union_ialloc_internal(sb, type);

  empty->vfs_inode.type = type;
  empty->vfs_inode.sb = sb;
  empty->vfs_inode.i_op = &unionfs_inode_ops;

  memset(&empty->underlying, 0, sizeof(empty->underlying));
  if (type == T_FILE) {
    empty->underlying.file.underlying_inode = underlying_inode;
    empty->underlying.file.layer_index = UNION_FS_TOP_LAYER_INDEX;
  } else {
    // allocation is on the top layer.
    empty->underlying.dir.inodes[UNION_FS_TOP_LAYER_INDEX] = underlying_inode;
  }


  // TODO: What fields should be copied from the underlying inode?
  return &empty->vfs_inode;
}


static int union_writei(struct vfs_inode *ip, char *src, uint off, uint n) {
  // TODO: Implement copy-up if not on top layer.
  if (ip->type != T_FILE) {
    panic("union_writei not file unsupported.");
  }
  struct unionfs_inode *uip = container_of(ip, struct unionfs_inode, vfs_inode);
  if (uip->underlying.file.layer_index != UNION_FS_TOP_LAYER_INDEX) {
    // Copy-up. Allocate new inode on top layer, copy data, and update inode.
    struct vfs_inode *top_layer_inode = union_ialloc(ip->sb, T_FILE);
    uip->underlying.file.underlying_inode = top_layer_inode;
    uip->underlying.file.layer_index = UNION_FS_TOP_LAYER_INDEX;
    return top_layer_inode->i_op->writei(top_layer_inode, src, off, n);
  } else {
    struct vfs_inode *top = top_inode(ip);
    return top->i_op->writei(top, src, off, n);
  }
}

static int unionfs_dirlink(struct vfs_inode *dip, char *name, struct vfs_inode *ip) {
  if (dip->type != T_DIR) panic("dirlink not DIR");
  // Mkdir on top layer if not already there.
  // Link on top layer.
  return 1;
}

static void union_iupdate(struct vfs_inode *ip) {
  struct unionfs_inode *uip = container_of(ip, struct unionfs_inode, vfs_inode);
  XV6_ASSERT(uip->vfs_inode.type == T_FILE && uip->underlying.file.layer_index == UNION_FS_TOP_LAYER_INDEX);
  struct vfs_inode *top = top_inode(ip);
  top->i_op->iupdate(top);
}

// General TODO: How do we make sure that the inode is not being used by another process?
// Maybe when we lock/unlock/update/etc. we should also lock/unlock the underlying inode?
// Check all cases!

static struct inode_operations unionfs_inode_ops = {
  .idup = union_idup,
  .dirlink = unionfs_dirlink,
  .dirlookup = union_dirlookup,
  .ilock = union_ilock,
  .iput = union_iput,
  .iunlock = union_iunlock,
  .iunlockput = union_iunlockput,
  .iupdate = union_iupdate,
  .readi = union_readi,
  .stati = union_stati,
  .writei = union_writei,
  .isdirempty = union_isdirempty,
};

static void unionfs_destroy(struct vfs_superblock *sb) {
  struct unionfs_superblock_private *sb_private = union_sb_private(sb);
  for (int i = 0; i < UNION_FS_MAX_LAYERS; ++i) {
    if (sb_private->options.layers_inodes[i].inode == NULL) continue;
    // release inode
    struct vfs_inode *layer_inode = sb_private->options.layers_inodes[i].inode;
    layer_inode->i_op->iput(layer_inode);
  }
  kfree((char *)sb_private);
  sb->private = NULL;
}



static const struct sb_ops unionfs_ops = {
  .destroy = unionfs_destroy,
  .ialloc = union_ialloc,
  .start = NULL,
};

void union_iinit() {
  initlock(&union_icache.lock, "union_icache");
  for (int i = 0; i < NINODE; ++i) {
    initsleeplock(&union_icache.inode[i].vfs_inode.lock, "union_inode");
  }
}

int unionfs_init(struct vfs_superblock *sb, const char* options_str) {
  int res = -1;
  XV6_ASSERT(PGSIZE >= sizeof(struct unionfs_superblock_private));
  struct unionfs_superblock_private *sb_private = (struct unionfs_superblock_private *)kalloc();
  if (sb_private == NULL) {
    cprintf("unionfs: failed to allocate superblock private\n");
    return -1;
  }

  // parse options
  if ((res = unionfs_parse_options(options_str, &sb_private->options)) != 0) {
      goto end;
  }

  sb->private = sb_private;
  sb->ops = &unionfs_ops;
  sb->root_ip = sb->ops->ialloc(sb, T_DIR);
  // Fill root inode with the top-layer inode
  // initialize private fields
  // success.
  res = 0;

end:
if (res != 0) {
  if (sb_private != NULL) {
    kfree((char *)sb_private);
  }
}
  return res;
}