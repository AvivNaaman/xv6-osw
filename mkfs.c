#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define stat xv6_stat  // avoid clash with host struct stat
#include "include/fsdefs.h"
#include "include/param.h"
#include "include/stat.h"
#include "include/types.h"

#ifndef static_assert
#define static_assert(a, b) \
  do {                      \
    switch (0)              \
    case 0:                 \
    case (a):;              \
  } while (0)
#endif

#define NINODES 600

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct native_superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, struct native_dinode *);
void rinode(uint inum, struct native_dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(file_type type);
void iappend(uint inum, void *p, int n);

// convert to intel byte order
ushort xshort(ushort x) {
  ushort y;
  uchar *a = (uchar *)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint xint(uint x) {
  uint y;
  uchar *a = (uchar *)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

void printusageexit(void) {
  fprintf(stderr, "Usage: mkfs fs.img <is_internal (0/1)> files...\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct native_dinode din;

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if (argc < 3) {
    printusageexit();
  }

  if (strlen(argv[2]) != 1 || (argv[2][0] != '0' && argv[2][0] != '1')) {
    printusageexit();
  }

  int is_internal = argv[2][0] == '1';

  int fssize = is_internal ? INT_FSSIZE : FSSIZE;
  int nbitmap = fssize / (BSIZE * 8) + 1;

  assert((BSIZE % sizeof(struct native_dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fsfd < 0) {
    perror(argv[1]);
    exit(1);
  }

  // 1 fs block = 1 disk sector
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = fssize - nmeta;

  sb.size = xint(fssize);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2 + nlog);
  sb.bmapstart = xint(2 + nlog + ninodeblocks);

  printf(
      "nmeta %d (boot, super, log blocks %d inode blocks %d, bitmap blocks %d) "
      "blocks %d total %d\n",
      nmeta, nlog, ninodeblocks, nbitmap, nblocks, fssize);

  freeblock = nmeta;  // the first free block that we can allocate

  for (i = 0; i < fssize; i++) wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  for (i = 3; i < argc; i++) {
    const char *full_path = argv[i];
    const char *base_name = strrchr(full_path, '/');
    if (!base_name)
      base_name = full_path;  // No slashes, we already have a base name
    else
      ++base_name;  // Skip one past the last '/'

    if ((fd = open(full_path, 0)) < 0) {
      perror(argv[i]);
      exit(1);
    }

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    if (base_name[0] == '_') ++base_name;

    inum = ialloc(T_FILE);

    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, base_name, DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    while ((cc = read(fd, buf, sizeof(buf))) > 0) iappend(inum, buf, cc);

    close(fd);
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off / BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  balloc(freeblock);

  exit(0);
}

void wsect(uint sec, void *buf) {
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    perror("lseek");
    exit(1);
  }
  if (write(fsfd, buf, BSIZE) != BSIZE) {
    perror("write");
    exit(1);
  }
}

void winode(uint inum, struct native_dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct native_dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct native_dinode *)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void rinode(uint inum, struct native_dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct native_dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct native_dinode *)buf) + (inum % IPB);
  *ip = *dip;
}

void rsect(uint sec, void *buf) {
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    perror("lseek");
    exit(1);
  }
  if (read(fsfd, buf, BSIZE) != BSIZE) {
    perror("read");
    exit(1);
  }
}

uint ialloc(file_type type) {
  uint inum = freeinode++;
  struct native_dinode din;

  bzero(&din, sizeof(din));
  din.base_dinode.type = xshort(type);
  din.base_dinode.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void balloc(int used) {
  uchar buf[BSIZE];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < BSIZE * 8);
  bzero(buf, BSIZE);
  for (i = 0; i < used; i++) {
    buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
  }
  printf("balloc: write bitmap block at sector %u\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void *xp, int n) {
  char *p = (char *)xp;
  uint fbn, off, n1;
  struct native_dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  // printf("append inum %d at off %d sz %d\n", inum, off, n);
  while (n > 0) {
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);
    if (fbn < NDIRECT) {
      if (xint(din.addrs[fbn]) == 0) {
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if (xint(din.addrs[NDIRECT]) == 0) {
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      if (indirect[fbn - NDIRECT] == 0) {
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      }
      x = xint(indirect[fbn - NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
