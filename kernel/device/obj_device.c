#include "devices.h"

struct device* create_obj_device() {
  acquire(&dev_holder.lock);
  struct device* dev = _get_new_device(DEVICE_TYPE_OBJ);
  release(&dev_holder.lock);
  return dev;
}
