// Component tests for the LD_PRELOAD shim in src/udev-spoof.c. The test binary
// links the real build/libudev-sony-spoof.so and runs against
// tests/stubs/libudev_stub.c (found through LD_LIBRARY_PATH), so the shim's
// dlopen("libudev.so.1") reaches the stub instead of the system libudev.
//
// "KnownIssue" tests pin documented weaknesses (docs/security-findings.md):
// they pass while the weakness exists and fail once it is fixed.
#include <gtest/gtest.h>

#include <csignal>
#include <cstdint>
#include <cstring>
#include <regex>
#include <string>

extern "C" {
struct udev_device {
    const char *vid;
    const char *pid;
    const char *devpath;
    const char *uevent;
};
udev_device *udev_device_get_parent_with_subsystem_devtype(udev_device *, const char *, const char *);
const char *udev_device_get_subsystem(udev_device *);
const char *udev_device_get_sysattr_value(udev_device *, const char *);
}

namespace {

constexpr char kVirtualPath[] = "/devices/virtual/misc/uhid/0003:054C:05C4.0001/hidraw/hidraw3";
constexpr char kRealPath[] = "/devices/pci0000:00/usb1/1-2/1-2:1.3/hidraw/hidraw4";

udev_device virtual_ds4() { return {"054c", "05c4", kVirtualPath, "DEVTYPE=usb_device\n"}; }
udev_device virtual_dualsense() { return {"054c", "0ce6", kVirtualPath, "DEVTYPE=usb_device\n"}; }
udev_device physical_ds4() { return {"054c", "05c4", kRealPath, "DEVTYPE=usb_device\n"}; }

bool is_tagged(const udev_device *d) { return (reinterpret_cast<uintptr_t>(d) & 1) != 0; }

// The spoofed parent of `dev`, the way a game's libudev call chain sees it.
udev_device *fake_parent(udev_device *dev) {
    return udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
}

} // namespace

TEST(UdevSpoof, VirtualSonyControllerGetsMarkedFakeUsbParent) {
    udev_device dev = virtual_ds4();
    udev_device *parent = fake_parent(&dev);
    ASSERT_NE(parent, nullptr);
    EXPECT_TRUE(is_tagged(parent));
    EXPECT_STREQ(udev_device_get_subsystem(parent), "usb");
}

TEST(UdevSpoof, PhysicalControllerIsNotSpoofed) {
    udev_device dev = physical_ds4();
    EXPECT_EQ(fake_parent(&dev), nullptr); // stub libudev has no parent
    EXPECT_STREQ(udev_device_get_subsystem(&dev), "hidraw");
}

TEST(UdevSpoof, OtherSubsystemQueriesPassThrough) {
    udev_device dev = virtual_ds4();
    EXPECT_EQ(udev_device_get_parent_with_subsystem_devtype(&dev, "hid", nullptr), nullptr);
}

TEST(UdevSpoof, ReportsSonyStringsOnTheFakeParent) {
    udev_device ds4 = virtual_ds4();
    udev_device ds = virtual_dualsense();
    EXPECT_STREQ(udev_device_get_sysattr_value(fake_parent(&ds4), "manufacturer"), "Sony Computer Entertainment");
    EXPECT_STREQ(udev_device_get_sysattr_value(fake_parent(&ds4), "product"), "Wireless Controller");
    EXPECT_STREQ(udev_device_get_sysattr_value(fake_parent(&ds), "manufacturer"), "Sony Interactive Entertainment");
    EXPECT_STREQ(udev_device_get_sysattr_value(fake_parent(&ds), "product"), "DualSense Wireless Controller");
}

TEST(UdevSpoof, SerialIsAMacAddress) {
    udev_device dev = virtual_ds4();
    const char *serial = udev_device_get_sysattr_value(fake_parent(&dev), "serial");
    ASSERT_NE(serial, nullptr);
    // Either the daemon's published MAC or the built-in default.
    EXPECT_TRUE(std::regex_match(serial, std::regex("([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}"))) << serial;
}

TEST(UdevSpoof, UeventGainsProductLine) {
    udev_device dev = virtual_ds4();
    const std::string uevent = udev_device_get_sysattr_value(fake_parent(&dev), "uevent");
    EXPECT_NE(uevent.find("DEVTYPE=usb_device"), std::string::npos);
    EXPECT_NE(uevent.find("PRODUCT=054c/05c4/0100"), std::string::npos);
}

TEST(UdevSpoof, RealDeviceSysattrPassesThrough) {
    udev_device dev = physical_ds4();
    EXPECT_STREQ(udev_device_get_sysattr_value(&dev, "uevent"), "DEVTYPE=usb_device\n");
    EXPECT_EQ(udev_device_get_sysattr_value(&dev, "serial"), nullptr);
}

// ------------------------------------------------------------ known issues

// L3: a NULL attribute name on the fake parent goes straight into strcmp().
TEST(UdevSpoofDeathTest, KnownIssue_L3_NullSysattrCrashes) {
    udev_device dev = virtual_ds4();
    udev_device *parent = fake_parent(&dev);
    EXPECT_EXIT((void)udev_device_get_sysattr_value(parent, nullptr), ::testing::KilledBySignal(SIGSEGV), "")
        << "NULL sysattr handled? turn this into a regression test";
}

// L3: the spoofed uevent text lives in one static buffer shared by every
// caller and thread, so concurrent queries overwrite each other.
TEST(UdevSpoof, KnownIssue_L3_UeventBufferIsSharedAcrossCalls) {
    udev_device a = virtual_ds4();
    udev_device b = {"054c", "05c4", kVirtualPath, "OTHER=device\n"};
    const char *first = udev_device_get_sysattr_value(fake_parent(&a), "uevent");
    const char *second = udev_device_get_sysattr_value(fake_parent(&b), "uevent");
    EXPECT_EQ(first, second) << "per-call buffers? turn this into a regression test";
    EXPECT_NE(std::string(first).find("OTHER=device"), std::string::npos); // first result was clobbered
}
