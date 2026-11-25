// Unit tests for src/bt-suppression.h: when the daemon hands an unbound
// Bluetooth connection back to hid-playstation after a USB hot-swap.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bt-suppression.h"

namespace {

const char kBt[] = "0005:054C:05C4.0014";

// A stand-in for sysfs and hid-playstation: binding fails while a USB
// connection of the same pad holds the MAC, as the real driver does.
struct FakeBus {
    bool usb = false;
    bool bt_exists = true;
    bool bt_bound = false;
    std::vector<std::string> bind_attempts;

    BtSuppression make() {
        return BtSuppression({
            [this] { return usb; },
            [this](const std::string&) { return bt_exists; },
            [this](const std::string& id) {
                bind_attempts.push_back(id);
                bt_bound = !usb; // "File exists" while USB holds the MAC
                return bt_bound;
            },
        });
    }
};

} // namespace

TEST(BtSuppression, CablePulledGivesBluetoothBack) {
    FakeBus bus;
    BtSuppression bt = bus.make();
    bus.usb = true;
    bt.suppress(kBt);

    bus.usb = false; // the USB connection drops
    bt.release();
    EXPECT_TRUE(bus.bt_bound);
    EXPECT_FALSE(bt.pending());
}

// The reported bug: a type change while on USB rebound Bluetooth (refused,
// USB still holds the MAC) and forgot it, so pulling the cable afterwards
// left the pad without input until it was reconnected.
TEST(BtSuppression, Regression_TypeChangeOnUsbKeepsBluetoothForLater) {
    FakeBus bus;
    BtSuppression bt = bus.make();
    bus.usb = true;
    bt.suppress(kBt);

    bt.release(); // type change: the daemon drops its USB connection
    EXPECT_TRUE(bus.bind_attempts.empty()) << "a bind now can only be refused";
    EXPECT_TRUE(bt.pending());

    bus.usb = false; // later, the cable is pulled
    bt.release();
    EXPECT_TRUE(bus.bt_bound);
    EXPECT_FALSE(bt.pending());
}

TEST(BtSuppression, PollNoticesUsbLeavingWhileNoConnectionIsOpen) {
    FakeBus bus;
    BtSuppression bt = bus.make();
    bus.usb = true;
    bt.suppress(kBt);
    bt.release(); // e.g. release-physical while on USB

    bt.poll();
    EXPECT_FALSE(bus.bt_bound);
    bus.usb = false;
    bt.poll();
    EXPECT_TRUE(bus.bt_bound);
}

TEST(BtSuppression, RefusedBindIsRetried) {
    FakeBus bus;
    BtSuppression bt = bus.make();
    bt.suppress(kBt);
    bus.usb = true;

    bt.restore_now(); // refused: USB holds the MAC
    EXPECT_FALSE(bus.bt_bound);
    EXPECT_TRUE(bt.pending());

    bus.usb = false;
    bt.poll();
    EXPECT_TRUE(bus.bt_bound);
    EXPECT_FALSE(bt.pending());
}

TEST(BtSuppression, PadGoneFromBluetoothIsForgotten) {
    FakeBus bus;
    BtSuppression bt = bus.make();
    bus.usb = true;
    bt.suppress(kBt);

    bus.bt_exists = false;
    bus.usb = false;
    bt.poll();
    EXPECT_TRUE(bus.bind_attempts.empty());
    EXPECT_FALSE(bt.pending());
}

TEST(BtSuppression, NothingSuppressedNeverBinds) {
    FakeBus bus;
    BtSuppression bt = bus.make();
    bt.release();
    bt.poll();
    bt.restore_now();
    EXPECT_TRUE(bus.bind_attempts.empty());
}
