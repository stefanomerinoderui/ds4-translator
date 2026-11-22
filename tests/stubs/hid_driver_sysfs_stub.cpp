// Test stand-in for the sysfs write primitives that live in src/main.cpp.
// Lets src/hid-unbind-detect.cpp link into a test binary without the daemon,
// and never touches /sys.
#include <string>

#include "hid-driver-sysfs.h"

void write_hid_driver_sysfs(const char *, const std::string &) {}
void write_hid_driver_sysfs_for(const char *, const char *, const std::string &) {}
