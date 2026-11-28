// Unit tests for src/pairing.cpp: the 0x13 report, the BlueZ info file and the
// whole cable_pair flow against a fake pad and a temporary BlueZ state dir.
#include <gtest/gtest.h>

#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "pairing.h"

namespace fs = std::filesystem;

namespace {

// Report 0x12 of a pad whose MAC is 40:1b:5f:3e:98:c0 (stored reversed).
const uint8_t kPairingInfo[] = {0x12, 0xc0, 0x98, 0x3e, 0x5f, 0x1b, 0x40, 0x08,
                                           0x25, 0x00, 0xe9, 0xde, 0xea, 0x41, 0x78, 0x90};

class FakePad : public FeatureIo {
public:
    bool answers = true;
    bool accepts_set = true;
    std::vector<uint8_t> sent;

    int get(uint8_t id, uint8_t* buf, size_t len) override {
        if (!answers || id != 0x12) return -1;
        const size_t n = std::min(len, sizeof(kPairingInfo));
        std::memcpy(buf, kPairingInfo, n);
        return (int)n;
    }
    bool set(const uint8_t* buf, size_t len) override {
        if (!accepts_set) return false;
        sent.assign(buf, buf + len);
        return true;
    }
};

class CablePairTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string tmpl = (fs::temp_directory_path() / "ds4-pairing-XXXXXX").string();
        ASSERT_NE(mkdtemp(tmpl.data()), nullptr);
        dir = tmpl;
        fs::create_directory(dir / "90:78:41:EA:DE:E9");
        std::memset(key, 0xAB, sizeof(key));
    }
    void TearDown() override { fs::remove_all(dir); }

    std::string read(const fs::path& p) {
        std::ifstream in(p);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    fs::path dir;
    uint8_t key[BT_LINK_KEY_SIZE];
    FakePad pad;
};

TEST(PairingFormat, ParsesMacInEitherCase) {
    uint8_t mac[6];
    ASSERT_TRUE(parse_bt_mac("90:78:41:ea:DE:e9", mac));
    EXPECT_EQ(mac[0], 0x90);
    EXPECT_EQ(mac[3], 0xea);
    EXPECT_EQ(mac[5], 0xe9);
}

TEST(PairingFormat, RejectsMalformedMacs) {
    uint8_t mac[6];
    EXPECT_FALSE(parse_bt_mac("", mac));
    EXPECT_FALSE(parse_bt_mac("90:78:41:ea:de", mac));
    EXPECT_FALSE(parse_bt_mac("90-78-41-ea-de-e9", mac));
    EXPECT_FALSE(parse_bt_mac("90:78:41:ea:de:zz", mac));
    EXPECT_FALSE(parse_bt_mac("90:78:41:ea:de:e9:", mac));
}

TEST(PairingFormat, ReportHasReversedHostMacThenKey) {
    const uint8_t host[6] = {0x90, 0x78, 0x41, 0xea, 0xde, 0xe9};
    uint8_t key[BT_LINK_KEY_SIZE];
    for (size_t i = 0; i < sizeof(key); ++i) key[i] = (uint8_t)i;
    uint8_t out[DS4_PAIRING_SET_SIZE];
    build_ds4_pairing_report(host, key, out);
    EXPECT_EQ(out[0], 0x13);
    const uint8_t reversed[6] = {0xe9, 0xde, 0xea, 0x41, 0x78, 0x90};
    EXPECT_EQ(std::memcmp(out + 1, reversed, 6), 0);
    EXPECT_EQ(std::memcmp(out + 7, key, sizeof(key)), 0);
}

TEST(PairingFormat, InfoFileCarriesTheKeyInUpperCaseHex) {
    uint8_t key[BT_LINK_KEY_SIZE];
    for (size_t i = 0; i < sizeof(key); ++i) key[i] = (uint8_t)(0xF0 + i);
    const std::string info = bluez_ds4_device_info(key);
    EXPECT_NE(info.find("[LinkKey]\nKey=F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF\n"), std::string::npos);
    EXPECT_NE(info.find("Trusted=true\n"), std::string::npos);
}

TEST_F(CablePairTest, WritesInfoFileAndPadReport) {
    const std::string reply = cable_pair(pad, dir.string(), "", key);
    EXPECT_EQ(reply.rfind("OK:", 0), 0u) << reply;

    const fs::path info = dir / "90:78:41:EA:DE:E9" / "40:1B:5F:3E:98:C0" / "info";
    ASSERT_TRUE(fs::exists(info));
    EXPECT_NE(read(info).find("Key=ABABABABABABABABABABABABABABABAB"), std::string::npos);
    EXPECT_EQ(fs::status(info).permissions() & fs::perms::others_read, fs::perms::none);

    ASSERT_EQ(pad.sent.size(), DS4_PAIRING_SET_SIZE);
    EXPECT_EQ(pad.sent[0], 0x13);
    EXPECT_EQ(pad.sent[1], 0xe9); // adapter 90:78:41:EA:DE:E9, reversed
    EXPECT_EQ(pad.sent[6], 0x90);
    EXPECT_EQ(pad.sent[7], 0xAB);
}

TEST_F(CablePairTest, RejectedPadWriteLeavesNothingInBluez) {
    pad.accepts_set = false;
    const std::string reply = cable_pair(pad, dir.string(), "", key);
    EXPECT_EQ(reply.rfind("Error:", 0), 0u) << reply;
    EXPECT_FALSE(fs::exists(dir / "90:78:41:EA:DE:E9" / "40:1B:5F:3E:98:C0"));
}

TEST_F(CablePairTest, UnreadablePadChangesNothing) {
    pad.answers = false;
    EXPECT_EQ(cable_pair(pad, dir.string(), "", key).rfind("Error:", 0), 0u);
    EXPECT_TRUE(pad.sent.empty());
    EXPECT_TRUE(fs::is_empty(dir / "90:78:41:EA:DE:E9"));
}

TEST_F(CablePairTest, RefusesAPadBluezAlreadyKnows) {
    const fs::path known = dir / "90:78:41:EA:DE:E9" / "40:1B:5F:3E:98:C0";
    fs::create_directory(known);
    std::ofstream(known / "info") << "old";
    const std::string reply = cable_pair(pad, dir.string(), "", key);
    EXPECT_EQ(reply.rfind("Error:", 0), 0u);
    EXPECT_NE(reply.find("bluetoothctl remove 40:1B:5F:3E:98:C0"), std::string::npos);
    EXPECT_EQ(read(known / "info"), "old");
    EXPECT_TRUE(pad.sent.empty());
}

TEST_F(CablePairTest, SeveralAdaptersNeedAnExplicitChoice) {
    fs::create_directory(dir / "11:22:33:44:55:66");
    EXPECT_EQ(cable_pair(pad, dir.string(), "", key).rfind("Error:", 0), 0u);
    EXPECT_TRUE(pad.sent.empty());

    const std::string reply = cable_pair(pad, dir.string(), "11:22:33:44:55:66", key);
    EXPECT_EQ(reply.rfind("OK:", 0), 0u) << reply;
    EXPECT_TRUE(fs::exists(dir / "11:22:33:44:55:66" / "40:1B:5F:3E:98:C0" / "info"));
}

TEST_F(CablePairTest, RejectsUnknownOrMalformedAdapter) {
    EXPECT_EQ(cable_pair(pad, dir.string(), "11:22:33:44:55:66", key).rfind("Error:", 0), 0u);
    EXPECT_EQ(cable_pair(pad, dir.string(), "../etc", key).rfind("Error:", 0), 0u);
}

TEST_F(CablePairTest, NoAdapterIsAnError) {
    fs::remove_all(dir / "90:78:41:EA:DE:E9");
    EXPECT_EQ(cable_pair(pad, dir.string(), "", key).rfind("Error:", 0), 0u);
}

} // namespace
