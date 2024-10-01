#include "cgroupstests.h"

#include "fcntl.h"
#include "framework/test.h"
#include "kernel/mmu.h"
#include "param.h"
#include "types.h"
#include "user/lib/mutex.h"
#include "user/lib/user.h"

INIT_TESTS_PLATFORM();

TEST(test_mount_union_fs) {
  // Lower dir setup
  ASSERT_FALSE(mkdir("/ut_low"));
  int f = open("/ut_low/file", O_CREATE | O_RDWR);
  ASSERT_TRUE(f > 0);
  ASSERT_TRUE(write(f, "l0f\n", 4) > 0);
  ASSERT_FALSE(close(f));

  // Upper dir setup
  ASSERT_FALSE(mkdir ("/ut_up"));

  // Mount:
  ASSERT_FALSE(mkdir ("/ut"));
  ASSERT_FALSE(mount("/ut_up;/ut_low", "/ut", "union"));
}

int main() {
    run_test(test_mount_union_fs);

    unlink("/ut_low/file");
    unlink("/ut_low");
    unlink("/ut_up");
    unlink("/ut");

  PRINT_TESTS_RESULT("UNIONFSTESTS");
  return CURRENT_TESTS_RESULT();
}
