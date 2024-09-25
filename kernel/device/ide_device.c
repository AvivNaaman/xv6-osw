#include "devices.h"

struct device* get_ide_device(const uint ide_port) {
  acquire(&dev_holder.lock);
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    if (dev->private != NULL && dev->private == (void*)ide_port &&
        dev->type == DEVICE_TYPE_IDE) {
      dev->ref++;
      release(&dev_holder.lock);
      return dev;
    }
  }
  release(&dev_holder.lock);
  return NULL;
}

struct device* create_ide_device(const uint ide_port) {
  acquire(&dev_holder.lock);
  struct device* dev = _get_new_device(DEVICE_TYPE_IDE);

  if (dev == NULL) {
    goto end;
  }

  dev->private = (void*)ide_port;
  dev->ops = &default_device_ops;

end:
  release(&dev_holder.lock);
  return dev;
}
