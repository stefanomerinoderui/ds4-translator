// Unit tests for hid_uevent_monitor_read() in src/hid-unbind-detect.cpp.
// The reader takes any fd, so datagrams are fed through a socketpair instead of
// a real NETLINK_KOBJECT_UEVENT socket.
//
// "KnownIssue" tests pin documented weaknesses (docs/security-findings.md):
// they pass while the weakness exists and fail once it is fixed, so the fix
// has to turn them into regression tests.
#include <gtest/gtest.h>

#include <initializer_list>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

#include "hid-unbind-detect.h"

namespace {

// Builds the kernel's wire format: "ACTION@DEVPATH\0KEY=VALUE\0...".
std::string uevent(const std::string &header, std::initializer_list<std::string> fields) {
    std::string out = header;
    out.push_back('\0');
    for (const std::string &f : fields) {
        out += f;
        out.push_back('\0');
    }
    return out;
}

class UeventFeed : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, fds_), 0); }
    void TearDown() override {
        close(fds_[0]);
        close(fds_[1]);
    }

    // Sends one datagram and returns what the reader makes of it.
    bool feed(const std::string &datagram, std::string *id = nullptr) {
        EXPECT_EQ(send(fds_[0], datagram.data(), datagram.size(), 0),
                  static_cast<ssize_t>(datagram.size()));
        std::string tmp;
        return hid_uevent_monitor_read(fds_[1], id != nullptr ? *id : tmp);
    }

    int fds_[2] = {-1, -1};
};

const char kUsbPath[] = "/devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2:1.3/0003:054C:05C4.0042";

std::string ds4_add(const std::string &devpath = kUsbPath, const std::string &hid_id = "0003:0000054C:000005C4") {
    return uevent("add@" + devpath, {"ACTION=add", "DEVPATH=" + devpath, "SUBSYSTEM=hid", "HID_ID=" + hid_id});
}

} // namespace

TEST_F(UeventFeed, AcceptsPhysicalDs4OverUsb) {
    std::string id;
    ASSERT_TRUE(feed(ds4_add(), &id));
    EXPECT_EQ(id, "0003:054C:05C4.0042");
}

TEST_F(UeventFeed, AcceptsBluetoothAndDs4V2) {
    std::string id;
    const std::string bt = "/devices/virtual/misc/uhid/0005:054C:09CC.0007";
    ASSERT_TRUE(feed(ds4_add(bt, "0005:0000054C:000009CC"), &id));
    EXPECT_EQ(id, "0005:054C:09CC.0007");
}

TEST_F(UeventFeed, IgnoresNonAddActions) {
    const std::string dev = kUsbPath;
    EXPECT_FALSE(feed(uevent("remove@" + dev, {"ACTION=remove", "DEVPATH=" + dev, "SUBSYSTEM=hid",
                                              "HID_ID=0003:0000054C:000005C4"})));
    EXPECT_FALSE(feed(uevent("change@" + dev, {"ACTION=change", "DEVPATH=" + dev, "SUBSYSTEM=hid",
                                              "HID_ID=0003:0000054C:000005C4"})));
}

TEST_F(UeventFeed, IgnoresOtherSubsystemsAndDevices) {
    const std::string dev = kUsbPath;
    EXPECT_FALSE(feed(uevent("add@" + dev, {"ACTION=add", "DEVPATH=" + dev, "SUBSYSTEM=usb",
                                           "HID_ID=0003:0000054C:000005C4"})));
    EXPECT_FALSE(feed(ds4_add(kUsbPath, "0003:0000046D:0000C52B"))); // a Logitech receiver
    EXPECT_FALSE(feed(ds4_add(kUsbPath, "0006:0000054C:000005C4"))); // unsupported bus
}

TEST_F(UeventFeed, IgnoresIncompleteEvents) {
    const std::string dev = kUsbPath;
    EXPECT_FALSE(feed(uevent("add@" + dev, {"ACTION=add", "SUBSYSTEM=hid", "HID_ID=0003:0000054C:000005C4"})));
    EXPECT_FALSE(feed(uevent("add@" + dev, {"ACTION=add", "DEVPATH=" + dev, "SUBSYSTEM=hid"})));
    EXPECT_FALSE(feed(""));
}

TEST_F(UeventFeed, IgnoresTheDaemonsOwnVirtualDevices) {
    // FunctionFS gadget under dummy_hcd, and uhid on bus 0003: same VID/PID by
    // design, so picking them up would make the daemon grab itself.
    EXPECT_FALSE(feed(ds4_add("/devices/platform/dummy_hcd.0/usb2/2-1/2-1:1.0/0003:054C:05C4.0001")));
    EXPECT_FALSE(feed(ds4_add("/devices/virtual/misc/uhid/0003:054C:05C4.0002")));
}

TEST_F(UeventFeed, ToleratesTruncatedAndUnterminatedDatagrams) {
    std::string datagram = ds4_add();
    datagram.pop_back(); // drop the final NUL
    (void)feed(datagram);
    (void)feed(datagram.substr(0, datagram.size() / 2));
    (void)feed(std::string(8192, 'A')); // larger than the reader's buffer
}

TEST_F(UeventFeed, DatagramIsAlwaysConsumed) {
    // A non-matching event must not stay queued, or poll() would spin on it.
    (void)feed(uevent("add@/x", {"ACTION=add"}));
    char probe;
    EXPECT_EQ(recv(fds_[1], &probe, 1, MSG_DONTWAIT), -1);
}

// ------------------------------------------------------------ known issues

// H4: the reader recv()s from any peer. Nothing checks the sender is the
// kernel (nl_pid == 0 / SCM_CREDENTIALS), so on a real netlink socket any
// local process can unicast a forged event. Here an ordinary socket pair
// stands in for "not the kernel", and the event is still accepted.
TEST_F(UeventFeed, KnownIssue_H4_AcceptsEventFromNonKernelSender) {
    std::string id;
    EXPECT_TRUE(feed(ds4_add(), &id)) << "sender check added? turn this into a regression test";
}

// H4: HID_ID is matched by substring, so surrounding junk is accepted.
TEST_F(UeventFeed, KnownIssue_H4_HidIdMatchedBySubstring) {
    EXPECT_TRUE(feed(ds4_add(kUsbPath, "0003:XX0000054CXX:XX000005C4XX")))
        << "strict HID_ID parsing added? turn this into a regression test";
}

// H4: the id handed to sysfs writes is just the last DEVPATH component; its
// shape is never validated.
TEST_F(UeventFeed, KnownIssue_H4_DevpathTailIsNotValidatedAsHidId) {
    std::string id;
    EXPECT_TRUE(feed(ds4_add("/devices/anything/not-a-hid-id"), &id))
        << "devpath validation added? turn this into a regression test";
    EXPECT_EQ(id, "not-a-hid-id");
}
