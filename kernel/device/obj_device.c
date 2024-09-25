#include "devices.h"

struct device* create_obj_device() {
  return get_new_device(DEVICE_TYPE_OBJ);
}
