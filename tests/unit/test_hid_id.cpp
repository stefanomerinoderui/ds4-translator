// Unit tests for parse_hid_id() in src/hid-unbind-detect.cpp.
#include <gtest/gtest.h>

#include <string>

#include "hid-unbind-detect.h"

namespace {

struct Parsed {
    bool ok;
    unsigned bus = 0, vendor = 0, product = 0;
};

Parsed parse(const std::string &id) {
    Parsed p{};
    p.ok = parse_hid_id(id, p.bus, p.vendor, p.product);
    return p;
}

} // namespace

TEST(ParseHidId, UsbDualShock4) {
    const Parsed p = parse("0003:054C:05C4.0042");
    ASSERT_TRUE(p.ok);
    EXPECT_EQ(p.bus, 0x0003u);
    EXPECT_EQ(p.vendor, 0x054Cu);
    EXPECT_EQ(p.product, 0x05C4u);
}

TEST(ParseHidId, BluetoothDualSense) {
    const Parsed p = parse("0005:054C:0CE6.000A");
    ASSERT_TRUE(p.ok);
    EXPECT_EQ(p.bus, 0x0005u);
    EXPECT_EQ(p.product, 0x0CE6u);
}

TEST(ParseHidId, InstanceSuffixIsOptionalAndLowercaseHexWorks) {
    const Parsed p = parse("0003:054c:09cc");
    ASSERT_TRUE(p.ok);
    EXPECT_EQ(p.vendor, 0x054Cu);
    EXPECT_EQ(p.product, 0x09CCu);
}

TEST(ParseHidId, RejectsMalformed) {
    for (const char *bad : {"", "0003", "0003:054C", "0003:054C:", ":054C:05C4", "0003::05C4",
                            "zzzz:054C:05C4.0001", "0003:05G4:05C4.0001", "0003:054C:05C4X.0001",
                            "0003:054C:99999999999999999999.0001"}) {
        EXPECT_FALSE(parse(bad).ok) << bad;
    }
}

TEST(ParseHidId, FailureLeavesOutputsUntouched) {
    unsigned bus = 7, vendor = 8, product = 9;
    EXPECT_FALSE(parse_hid_id("garbage", bus, vendor, product));
    EXPECT_EQ(bus, 7u);
    EXPECT_EQ(vendor, 8u);
    EXPECT_EQ(product, 9u);
}
