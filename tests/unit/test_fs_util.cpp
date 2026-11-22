// Unit tests for ds4fs::write_file_atomic(): the helper the daemon uses for its
// config, state and sentinel files.
#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "fs-util.h"

namespace {

class AtomicWrite : public ::testing::Test {
protected:
    void SetUp() override {
        char tmpl[] = "/tmp/ds4-fs-test-XXXXXX";
        ASSERT_NE(mkdtemp(tmpl), nullptr);
        dir_ = tmpl;
    }
    void TearDown() override {
        const std::string cmd = "rm -rf '" + dir_ + "'";
        (void)!system(cmd.c_str());
    }

    static std::string slurp(const std::string &path) {
        std::ifstream f(path);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    std::string dir_;
};

} // namespace

TEST_F(AtomicWrite, CreatesFileWithContentAndExactMode) {
    const std::string path = dir_ + "/conf";
    std::string error;
    ASSERT_TRUE(ds4fs::write_file_atomic(path, "type=ds4\n", 0644, &error)) << error;
    EXPECT_EQ(slurp(path), "type=ds4\n");
    struct stat st;
    ASSERT_EQ(stat(path.c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0644u);
    EXPECT_NE(access((path + ".tmp").c_str(), F_OK), 0) << "temp file must not be left behind";
}

TEST_F(AtomicWrite, ReplacesExistingContent) {
    const std::string path = dir_ + "/conf";
    ASSERT_TRUE(ds4fs::write_file_atomic(path, std::string(5000, 'x'), 0644, nullptr));
    ASSERT_TRUE(ds4fs::write_file_atomic(path, "short", 0644, nullptr));
    EXPECT_EQ(slurp(path), "short");
}

TEST_F(AtomicWrite, WritesEmptyFile) {
    const std::string path = dir_ + "/sentinel";
    ASSERT_TRUE(ds4fs::write_file_atomic(path, "", 0644, nullptr));
    struct stat st;
    ASSERT_EQ(stat(path.c_str(), &st), 0);
    EXPECT_EQ(st.st_size, 0);
}

// Regression for L2: a symlink planted at the destination must be replaced,
// never written through.
TEST_F(AtomicWrite, Regression_L2_SymlinkAtDestinationIsReplacedNotFollowed) {
    const std::string victim = dir_ + "/victim";
    const std::string path = dir_ + "/conf";
    ASSERT_EQ(symlink(victim.c_str(), path.c_str()), 0);
    ASSERT_TRUE(ds4fs::write_file_atomic(path, "data", 0644, nullptr));
    EXPECT_NE(access(victim.c_str(), F_OK), 0) << "write followed the symlink";
    struct stat st;
    ASSERT_EQ(lstat(path.c_str(), &st), 0);
    EXPECT_TRUE(S_ISREG(st.st_mode)) << "destination should now be a regular file";
}

// Same for the temp name: a symlink or stale file there must not redirect the write.
TEST_F(AtomicWrite, Regression_L2_PlantedTempFileDoesNotRedirectTheWrite) {
    const std::string victim = dir_ + "/victim";
    const std::string path = dir_ + "/conf";
    ASSERT_EQ(symlink(victim.c_str(), (path + ".tmp").c_str()), 0);
    ASSERT_TRUE(ds4fs::write_file_atomic(path, "data", 0644, nullptr));
    EXPECT_NE(access(victim.c_str(), F_OK), 0) << "write followed a symlink at the temp name";
    EXPECT_EQ(slurp(path), "data");
}

TEST_F(AtomicWrite, FailsWithMessageWhenDirectoryMissing) {
    std::string error;
    EXPECT_FALSE(ds4fs::write_file_atomic(dir_ + "/missing/conf", "x", 0644, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(ds4fs::write_file_atomic(dir_ + "/missing/conf", "x", 0644, nullptr)); // null error ok
}

TEST_F(AtomicWrite, OldContentSurvivesAFailedWrite) {
    const std::string path = dir_ + "/conf";
    ASSERT_TRUE(ds4fs::write_file_atomic(path, "good", 0644, nullptr));
    // Make the temp name unusable as a file (a directory): the write fails
    // without touching the destination.
    ASSERT_EQ(mkdir((path + ".tmp").c_str(), 0700), 0);
    // unlink() of a directory fails, so open(O_EXCL) fails too.
    EXPECT_FALSE(ds4fs::write_file_atomic(path, "bad", 0644, nullptr));
    EXPECT_EQ(slurp(path), "good");
}
