// Component tests for src/driver-transport.cpp against a scratch directory
// standing in for /dev/ds4-translator and /sys/module/hid_ds4_translator
// (DS4_DRIVER_DEV_DIR / DS4_DRIVER_MODULE_DIR). The success path needs a real
// node with the module's ioctl; tests/kernel/test-binder.sh covers it against
// the actual kernel module.
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

#include "driver-transport.h"

namespace {

using Clock = std::chrono::steady_clock;
constexpr auto kShortWait = std::chrono::milliseconds(300);
const char kHidId[] = "0003:054C:05C4.0001";

class DriverTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        char tmpl[] = "/tmp/ds4-driver-test-XXXXXX";
        const char *dir = mkdtemp(tmpl);
        ASSERT_NE(dir, nullptr);
        root_ = dir;
        dev_dir_ = root_ + "/dev";
        module_dir_ = root_ + "/module";
        ASSERT_EQ(mkdir(dev_dir_.c_str(), 0700), 0);
        setenv("DS4_DRIVER_DEV_DIR", dev_dir_.c_str(), 1);
        setenv("DS4_DRIVER_MODULE_DIR", module_dir_.c_str(), 1);
    }

    void TearDown() override {
        unsetenv("DS4_DRIVER_DEV_DIR");
        unsetenv("DS4_DRIVER_MODULE_DIR");
        const std::string cmd = "rm -rf '" + root_ + "'";
        EXPECT_EQ(system(cmd.c_str()), 0);
    }

    void make_node(const std::string &name) const { std::ofstream(dev_dir_ + "/" + name).put('x'); }

    std::string root_, dev_dir_, module_dir_;
};

TEST_F(DriverTransportTest, AvailableOnlyWhileTheModuleDirectoryExists) {
    EXPECT_FALSE(driver_transport_available());
    ASSERT_EQ(mkdir(module_dir_.c_str(), 0700), 0);
    EXPECT_TRUE(driver_transport_available());
}

TEST_F(DriverTransportTest, MissingNodeFailsAfterTheWait) {
    const auto start = Clock::now();
    EXPECT_EQ(driver_transport_open(kHidId, kShortWait), -1);
    EXPECT_GE(Clock::now() - start, kShortWait);
}

TEST_F(DriverTransportTest, NodeThatIsNotAnDs4tDeviceIsRejectedWithoutWaiting) {
    make_node(kHidId);
    const auto start = Clock::now();
    EXPECT_EQ(driver_transport_open(kHidId, std::chrono::seconds(5)), -1); // ENOTTY on the ioctl
    EXPECT_LT(Clock::now() - start, std::chrono::seconds(2));
}

TEST_F(DriverTransportTest, WaitsForTheNodeToAppear) {
    std::thread creator([this] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        make_node(kHidId);
    });
    const auto start = Clock::now();
    // The node shows up (so the wait ends early) but is not a real device.
    EXPECT_EQ(driver_transport_open(kHidId, std::chrono::seconds(5)), -1);
    creator.join();
    EXPECT_GE(Clock::now() - start, std::chrono::milliseconds(150));
    EXPECT_LT(Clock::now() - start, std::chrono::seconds(2));
}

TEST_F(DriverTransportTest, HidIdCannotEscapeTheDeviceDirectory) {
    make_node("victim");
    for (const char *bad : {"", "../dev/victim", "a/b", ".hidden"}) {
        const auto start = Clock::now();
        EXPECT_EQ(driver_transport_open(bad, std::chrono::seconds(5)), -1) << bad;
        EXPECT_LT(Clock::now() - start, std::chrono::seconds(1)) << bad;
    }
}

} // namespace
