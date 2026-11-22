// Component tests for the IPC transport in src/ipc-client.cpp, run against a
// fake daemon on a scratch Unix socket (DS4_RUNTIME_DIR), so the real socket
// path is never touched.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc-client.h"

using namespace ds4ipc;

namespace {

// One-connection fake daemon: runs `handler(client_fd)` for the first client.
class FakeDaemon {
public:
    explicit FakeDaemon(std::function<void(int)> handler) : handler_(std::move(handler)) {
        char tmpl[] = "/tmp/ds4-ipc-test-XXXXXX";
        const char *dir = mkdtemp(tmpl);
        EXPECT_NE(dir, nullptr);
        dir_ = dir != nullptr ? dir : "";
        setenv("DS4_RUNTIME_DIR", dir_.c_str(), 1);

        listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const std::string path = dir_ + "/ds4-translator.sock";
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
        EXPECT_EQ(bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);
        EXPECT_EQ(listen(listen_fd_, 1), 0);
        thread_ = std::thread([this] {
            const int client = accept(listen_fd_, nullptr, nullptr);
            if (client >= 0) {
                handler_(client);
                close(client);
            }
        });
    }

    ~FakeDaemon() {
        if (listen_fd_ >= 0) {
            shutdown(listen_fd_, SHUT_RDWR); // unblocks accept() if no client came
            close(listen_fd_);
        }
        if (thread_.joinable()) thread_.join();
        unlink((dir_ + "/ds4-translator.sock").c_str());
        rmdir(dir_.c_str());
        unsetenv("DS4_RUNTIME_DIR");
    }

private:
    std::function<void(int)> handler_;
    std::string dir_;
    int listen_fd_ = -1;
    std::thread thread_;
};

// The protocol is one unterminated command per connection, so a single read
// is the whole request.
std::string read_command(int fd) {
    char buf[1024];
    const ssize_t n = read(fd, buf, sizeof(buf));
    return n > 0 ? std::string(buf, static_cast<size_t>(n)) : std::string();
}

} // namespace

TEST(SendCommand, RoundTripAndTrailingNewlineTrimmed) {
    std::string received;
    FakeDaemon daemon([&](int fd) {
        received = read_command(fd);
        const char reply[] = "Virtual Emulation: DualShock 4\n\n";
        (void)!send(fd, reply, sizeof(reply) - 1, MSG_NOSIGNAL);
    });
    std::string response;
    ASSERT_TRUE(send_command("status", response, 2000));
    EXPECT_EQ(received, "status");
    EXPECT_EQ(response, "Virtual Emulation: DualShock 4");
}

TEST(SendCommand, FailsCleanlyWhenDaemonIsNotRunning) {
    char tmpl[] = "/tmp/ds4-ipc-test-XXXXXX";
    ASSERT_NE(mkdtemp(tmpl), nullptr);
    setenv("DS4_RUNTIME_DIR", tmpl, 1);
    std::string response;
    EXPECT_FALSE(send_command("status", response, 500));
    EXPECT_NE(response.find("Failed to connect"), std::string::npos);
    unsetenv("DS4_RUNTIME_DIR");
    rmdir(tmpl);
}

TEST(SendCommand, TimesOutOnSilentDaemon) {
    std::atomic<bool> release{false};
    FakeDaemon daemon([&](int fd) {
        (void)read_command(fd);
        while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
    std::string response;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(send_command("status", response, 300));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    release = true;
    EXPECT_LT(elapsed, std::chrono::seconds(5));
    EXPECT_FALSE(response.empty()); // carries the reason
}

TEST(SendCommand, LargeResponseIsReadToEof) {
    const std::string big(200000, 'x');
    FakeDaemon daemon([&](int fd) {
        (void)read_command(fd);
        size_t off = 0;
        while (off < big.size()) {
            const ssize_t n = send(fd, big.data() + off, big.size() - off, MSG_NOSIGNAL);
            if (n <= 0) break;
            off += static_cast<size_t>(n);
        }
    });
    std::string response;
    ASSERT_TRUE(send_command("status", response, 5000));
    EXPECT_EQ(response, big);
}

TEST(SendCommand, ClosedBeforeAnyReplyReportsFailureOrEmptyResponse) {
    FakeDaemon daemon([](int fd) { (void)read_command(fd); });
    std::string response;
    // Either outcome is acceptable; it must not hang or crash.
    (void)send_command("status", response, 2000);
}

// ------------------------------------------------------- read/write helpers

TEST(ReadAvailable, DataThenEofOnSocketPair) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    std::string err;
    ASSERT_TRUE(write_command(fds[0], "hello", &err)) << err;
    close(fds[0]);

    std::string out;
    EXPECT_EQ(read_available(fds[1], &out, &err), ReadStatus::Data);
    EXPECT_EQ(out, "hello"); // commands carry no terminator
    EXPECT_EQ(read_available(fds[1], &out, &err), ReadStatus::Eof);
    close(fds[1]);
}

TEST(ReadAvailable, AgainOnEmptyNonBlockingFd) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    std::string err;
    ASSERT_TRUE(set_non_blocking(fds[1], &err)) << err;
    std::string out;
    EXPECT_EQ(read_available(fds[1], &out, &err), ReadStatus::Again);
    close(fds[0]);
    close(fds[1]);
}

TEST(Regression, WriteToClosedPeerReturnsErrorInsteadOfRaisingSigpipe) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    close(fds[1]);
    std::string err;
    EXPECT_FALSE(write_command(fds[0], std::string(1 << 20, 'a'), &err));
    EXPECT_FALSE(err.empty());
    close(fds[0]);
}
