// Component tests for src/usb-hid-transport.cpp against the fake libusb in
// tests/stubs/libusb_stub.cpp. They guard finding M3: the watchdog thread
// used to libusb_close() the handle while the pump thread was inside a
// transfer on it, which libusb leaves undefined. Every scenario ends with
// fakeusb::violations == 0 (no use after close, no double close, no close
// under an in-flight transfer); run under ASan/TSan a regression is also a
// sanitizer report.
#include <gtest/gtest.h>

#include <chrono>
#include <random>
#include <thread>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../stubs/libusb_stub.h"
#include "usb-hid-transport.h"

namespace {

constexpr uint16_t kVendorSony = 0x054c;
constexpr uint16_t kProductDs4 = 0x05c4;
const char kHidId[] = "0003:0000054C:000005C4.0001";

// The transport's watchdog looks at the device every 700 ms.
constexpr auto kWatchdogPeriod = std::chrono::milliseconds(700);
constexpr auto kPatience = std::chrono::seconds(4);

using Clock = std::chrono::steady_clock;

class UsbTransportTest : public ::testing::Test {
protected:
    // A regression can hang a join; fail the run instead of stalling CI.
    void SetUp() override {
        fakeusb::reset();
        alarm(120);
    }
    void TearDown() override { alarm(0); }

    // Opens the fake pad and starts pumping; returns the daemon-side fd.
    int open_and_start() {
        const int fd = usb_hid_transport_open(kHidId, kVendorSony, kProductDs4);
        EXPECT_GE(fd, 0);
        if (fd >= 0) usb_hid_transport_start(fd);
        return fd;
    }

    // True once the transport's far end hung up (what main.cpp sees as the
    // controller disconnecting). Reports still queued are drained first.
    static bool wait_for_hangup(int fd, std::chrono::milliseconds budget) {
        const auto give_up = Clock::now() + budget;
        char buf[128];
        while (Clock::now() < give_up) {
            pollfd pfd{fd, POLLIN, 0};
            if (poll(&pfd, 1, 50) > 0) {
                const ssize_t n = recv(fd, buf, sizeof(buf), 0);
                if (n == 0) return true;
            }
        }
        return false;
    }
};

TEST_F(UsbTransportTest, ForwardsReportsFromTheDevice) {
    fakeusb::controls.feed = true;
    const int fd = open_and_start();
    ASSERT_GE(fd, 0);

    pollfd pfd{fd, POLLIN, 0};
    ASSERT_GT(poll(&pfd, 1, 2000), 0);
    char buf[128];
    EXPECT_EQ(recv(fd, buf, sizeof(buf), 0), 64);

    usb_hid_transport_close(fd);
    EXPECT_EQ(fakeusb::violations.load(), 0);
}

TEST_F(UsbTransportTest, CleanCloseHandsTheInterfaceBackAndClosesOnce) {
    const int fd = open_and_start();
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    usb_hid_transport_close(fd);

    EXPECT_EQ(fakeusb::violations.load(), 0);
    EXPECT_EQ(fakeusb::open_handles(), 0);
    EXPECT_EQ(fakeusb::release_calls.load(), 1);
    EXPECT_EQ(fakeusb::attach_calls.load(), 1);
}

TEST_F(UsbTransportTest, UnpluggedDeviceEndsTheTransportWithoutTouchingAClosedHandle) {
    const int fd = open_and_start();
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fakeusb::controls.gone = true;
    EXPECT_TRUE(wait_for_hangup(fd, kPatience));

    usb_hid_transport_close(fd);
    EXPECT_EQ(fakeusb::violations.load(), 0);
    EXPECT_EQ(fakeusb::open_handles(), 0);
}

// The case the watchdog exists for: the transfer never returns by itself.
TEST_F(UsbTransportTest, WedgedTransferIsUnstuckWhenTheDeviceDisappears) {
    fakeusb::controls.wedge = true;
    const int fd = open_and_start();
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Enumeration loses the device but the pending transfer never notices:
    // only the watchdog acting on the enumeration can end it.
    fakeusb::controls.gone = true;
    EXPECT_TRUE(wait_for_hangup(fd, kPatience));

    usb_hid_transport_close(fd);
    EXPECT_EQ(fakeusb::violations.load(), 0);
    EXPECT_EQ(fakeusb::open_handles(), 0);
}

// A transfer wedged while the daemon itself is shutting the transport down.
TEST_F(UsbTransportTest, CloseUnsticksAWedgedTransfer) {
    fakeusb::controls.wedge = true;
    const int fd = open_and_start();
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    usb_hid_transport_close(fd);

    EXPECT_EQ(fakeusb::violations.load(), 0);
    EXPECT_EQ(fakeusb::open_handles(), 0);
}

TEST_F(UsbTransportTest, ReclaimedInterfaceEndsTheTransport) {
    const int fd = open_and_start();
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fakeusb::controls.reclaimed = true;
    EXPECT_TRUE(wait_for_hangup(fd, kPatience));

    usb_hid_transport_close(fd);
    EXPECT_EQ(fakeusb::violations.load(), 0);
    EXPECT_EQ(fakeusb::open_handles(), 0);
}

// Closing at the same moment the watchdog fires is the window the original
// bug lived in: sweep the close time across it.
TEST_F(UsbTransportTest, CloseRacingTheWatchdogIsSafe) {
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> jitter_ms(-40, 40);

    for (int round = 0; round < 12; ++round) {
        fakeusb::reset();
        fakeusb::controls.wedge = (round % 2) == 0;
        const int fd = open_and_start();
        ASSERT_GE(fd, 0);

        fakeusb::controls.gone = true;
        std::this_thread::sleep_for(kWatchdogPeriod + std::chrono::milliseconds(jitter_ms(rng)));
        usb_hid_transport_close(fd);

        ASSERT_EQ(fakeusb::violations.load(), 0) << "round " << round;
        ASSERT_EQ(fakeusb::open_handles(), 0) << "round " << round;
    }
}

} // namespace
