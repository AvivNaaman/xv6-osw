#include "fs.h"

#include "native_fs.h"
#include "obj_fs.h"
#include "unionfs.h"

void fsinit() {
  native_iinit();
  obj_iinit();
  union_iinit();
}
