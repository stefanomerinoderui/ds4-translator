/* Minimal stand-in for libudev.so.1, for testing src/udev-spoof.c.
 *
 * The shim dlopen()s "libudev.so.1" to reach the real functions it wraps; the
 * test puts this library first on LD_LIBRARY_PATH so those lookups land here
 * and return whatever the test put in a struct udev_device.
 */
#include <stddef.h>
#include <string.h>

struct udev_device {
    const char *vid;
    const char *pid;
    const char *devpath;
    const char *uevent;
};

const char *udev_device_get_property_value(struct udev_device *dev, const char *key) {
    if (!dev || !key) return NULL;
    if (!strcmp(key, "ID_VENDOR_ID")) return dev->vid;
    if (!strcmp(key, "ID_MODEL_ID")) return dev->pid;
    if (!strcmp(key, "DEVPATH")) return dev->devpath;
    return NULL;
}

const char *udev_device_get_sysattr_value(struct udev_device *dev, const char *attr) {
    if (dev && attr && !strcmp(attr, "uevent")) return dev->uevent;
    return NULL;
}

struct udev_device *udev_device_get_parent_with_subsystem_devtype(struct udev_device *dev,
                                                                  const char *subsystem,
                                                                  const char *devtype) {
    (void)dev;
    (void)subsystem;
    (void)devtype;
    return NULL;
}

const char *udev_device_get_subsystem(struct udev_device *dev) {
    (void)dev;
    return "hidraw";
}
