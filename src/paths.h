#ifndef DS4_PATHS_H
#define DS4_PATHS_H

#include <cstdlib>
#include <string>

// Filesystem locations shared by the daemon and its clients, each with an
// environment override. The defaults are the production paths; the overrides
// exist so the daemon and ds4-ctl can run unprivileged against a scratch
// directory in tests. They are read from the *process* environment, so they
// grant nothing a caller that can already set the daemon's environment
// (root / systemd) did not have.
//
//   DS4_RUNTIME_DIR  default /run                  socket + "none" sentinel
//   DS4_STATE_DIR    default /var/lib/ds4-translator   remembered pad MAC
//   DS4_CONFIG_PATH  default /etc/ds4-translator.conf  persisted settings
//   DS4_DRIVER_DEV_DIR     default /dev/ds4-translator          per-pad nodes of the kernel module
//   DS4_DRIVER_MODULE_DIR  default /sys/module/hid_ds4_translator   exists while the module is loaded
namespace ds4paths {

inline std::string from_env(const char *name, const char *fallback) {
    const char *value = std::getenv(name);
    return (value != nullptr && *value != '\0') ? std::string(value) : std::string(fallback);
}

inline std::string runtime_dir() { return from_env("DS4_RUNTIME_DIR", "/run"); }
inline std::string socket_path() { return runtime_dir() + "/ds4-translator.sock"; }
inline std::string none_sentinel_path() { return runtime_dir() + "/ds4-translator.none"; }
inline std::string state_dir() { return from_env("DS4_STATE_DIR", "/var/lib/ds4-translator"); }
inline std::string config_path() { return from_env("DS4_CONFIG_PATH", "/etc/ds4-translator.conf"); }
inline std::string driver_dev_dir() { return from_env("DS4_DRIVER_DEV_DIR", "/dev/ds4-translator"); }
inline std::string driver_module_dir() { return from_env("DS4_DRIVER_MODULE_DIR", "/sys/module/hid_ds4_translator"); }

} // namespace ds4paths

#endif // DS4_PATHS_H
