#include "fs-util.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ds4fs {

namespace {

void set_error(std::string *error, const char *what) {
    if (error != nullptr) {
        *error = std::string(what) + ": " + strerror(errno);
    }
}

bool write_all(int fd, const char *data, size_t size) {
    size_t written = 0;
    while (written < size) {
        const ssize_t n = write(fd, data + written, size - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

} // namespace

bool write_file_atomic(const std::string &path, const std::string &content, unsigned mode,
                       std::string *error) {
    const std::string tmp = path + ".tmp";
    // A stale temp file (earlier crash) or a planted symlink at the temp name
    // is removed itself -- unlink() never follows a final symlink.
    (void)unlink(tmp.c_str());

    const int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, mode);
    if (fd < 0) {
        set_error(error, "open temp file");
        return false;
    }
    // open() applies the umask; make the mode exact.
    if (fchmod(fd, static_cast<mode_t>(mode)) != 0 || !write_all(fd, content.data(), content.size()) ||
        fsync(fd) != 0) {
        set_error(error, "write temp file");
        close(fd);
        (void)unlink(tmp.c_str());
        return false;
    }
    if (close(fd) != 0 || rename(tmp.c_str(), path.c_str()) != 0) {
        set_error(error, "rename temp file");
        (void)unlink(tmp.c_str());
        return false;
    }
    return true;
}

} // namespace ds4fs
