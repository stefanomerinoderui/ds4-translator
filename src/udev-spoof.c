// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp) -- glibc feature-test macro, not user code
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>
#include <pthread.h>

struct udev_device;

static struct udev_device *(*real_get_parent)(struct udev_device *, const char *, const char *) = NULL;
static const char *(*real_get_sysattr)(struct udev_device *, const char *) = NULL;
static const char *(*real_get_property)(struct udev_device *, const char *) = NULL;
static const char *(*real_get_subsystem)(struct udev_device *) = NULL;

// Resolved once, race-free: the shim runs inside whatever threads a game
// creates, and several can make their first udev call at the same moment.
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void do_init_functions(void) {
    {
        void *handle = dlopen("libudev.so.1", RTLD_LAZY);
        if (!handle) handle = dlopen("libudev.so", RTLD_LAZY);
        if (!handle) {
            (void)fprintf(stderr, "[udev-spoof] ERROR: Failed to load libudev.so.1 or libudev.so!\n");
            return;
        }
        real_get_parent = dlsym(handle, "udev_device_get_parent_with_subsystem_devtype");
        real_get_sysattr = dlsym(handle, "udev_device_get_sysattr_value");
        real_get_property = dlsym(handle, "udev_device_get_property_value");
        real_get_subsystem = dlsym(handle, "udev_device_get_subsystem");
        (void)fprintf(stderr, "[udev-spoof] Library initialized: parent=%p, sysattr=%p, property=%p, subsystem=%p\n",
                (void *)real_get_parent, (void *)real_get_sysattr, (void *)real_get_property, (void *)real_get_subsystem);
    }
}

static void init_functions(void) {
    (void)pthread_once(&init_once, do_init_functions);
}

static int is_virtual_sony_controller(struct udev_device *dev) {
    init_functions();
    if (!dev || !real_get_property) return 0;
    
    struct udev_device *clean_dev = (struct udev_device *)((uintptr_t)dev & ~(uintptr_t)1);
    const char *vid = real_get_property(clean_dev, "ID_VENDOR_ID");
    const char *pid = real_get_property(clean_dev, "ID_MODEL_ID");
    const char *devpath = real_get_property(clean_dev, "DEVPATH");
    
    int is_virtual = devpath && strstr(devpath, "/devices/virtual/misc/uhid/") != NULL;
    
    if (is_virtual && vid && !strcmp(vid, "054c")) {
        if (pid && (!strcmp(pid, "05c4") || !strcmp(pid, "0ce6"))) {
            return 1;
        }
    }
    return 0;
}

struct udev_device *udev_device_get_parent_with_subsystem_devtype(struct udev_device *udev_device, const char *subsystem, const char *devtype) {
    init_functions();
    
    if (subsystem && !strcmp(subsystem, "usb") && is_virtual_sony_controller(udev_device)) {
        (void)fprintf(stderr, "[udev-spoof] Spoofing parent query for virtual Sony controller. Returning marked fake parent.\n");
        return (struct udev_device *)((uintptr_t)udev_device | 1);
    }
    
    if ((uintptr_t)udev_device & 1) {
        struct udev_device *real_dev = (struct udev_device *)((uintptr_t)udev_device & ~(uintptr_t)1);
        if (real_get_parent) {
            return real_get_parent(real_dev, subsystem, devtype);
        }
        return NULL;
    }
    
    if (real_get_parent) {
        return real_get_parent(udev_device, subsystem, devtype);
    }
    return NULL;
}

const char *udev_device_get_subsystem(struct udev_device *udev_device) {
    init_functions();
    
    if ((uintptr_t)udev_device & 1) {
        (void)fprintf(stderr, "[udev-spoof] Spoofing subsystem query for fake parent: usb\n");
        return "usb";
    }
    
    if (real_get_subsystem) {
        return real_get_subsystem(udev_device);
    }
    return NULL;
}

/* The emulated device's serial is its MAC address, and that is no longer a
 * constant: when the daemon's MAC squat is armed the emulated device takes
 * over the physical pad's MAC so hid-playstation refuses to probe the real
 * one. This shim runs inside the game's process, not the daemon's, so the
 * live value is picked up from the file the daemon publishes. Falls back to
 * the historical constant when the daemon isn't running or hasn't written it.
 */
#define DEFAULT_VIRTUAL_MAC "74:e7:d6:3a:47:e8"

static char mac_serial[32];
static pthread_once_t mac_once = PTHREAD_ONCE_INIT;

static void load_virtual_mac_serial(void) {
    FILE *f = fopen("/run/ds4-translator/virtual-mac", "r");
    if (f) {
        if (fgets(mac_serial, sizeof(mac_serial), f)) {
            // Clamp explicitly: the analyzer treats strcspn()'s result as a
            // tainted index (file input) and can't see it is < sizeof(mac_serial).
            size_t len = strcspn(mac_serial, "\r\n");
            if (len >= sizeof(mac_serial)) len = sizeof(mac_serial) - 1;
            mac_serial[len] = '\0';
        }
        (void)fclose(f);
    }
    if (!mac_serial[0]) {
        (void)snprintf(mac_serial, sizeof(mac_serial), "%s", DEFAULT_VIRTUAL_MAC);
    }
}

static const char *virtual_mac_serial(void) {
    (void)pthread_once(&mac_once, load_virtual_mac_serial);
    return mac_serial;
}

/* The spoofed uevent text is built per call. A ring of per-thread buffers keeps
 * concurrent callers from overwriting each other (the old single static buffer
 * was shared by every thread) and lets a few results from the same thread stay
 * valid at once, which callers that hold on to the pointer rely on.
 */
#define SPOOF_UEVENT_SLOTS 8
#define SPOOF_UEVENT_SIZE 2048

static char *next_uevent_buffer(void) {
    static __thread char buffers[SPOOF_UEVENT_SLOTS][SPOOF_UEVENT_SIZE];
    static __thread unsigned next_slot;
    return buffers[next_slot++ % SPOOF_UEVENT_SLOTS];
}

const char *udev_device_get_sysattr_value(struct udev_device *udev_device, const char *sysattr) {
    init_functions();

    if (!sysattr) {
        return NULL; // libudev rejects this too; never hand it to strcmp()
    }

    if (((uintptr_t)udev_device & 1) && real_get_property) {
        struct udev_device *real_dev = (struct udev_device *)((uintptr_t)udev_device & ~(uintptr_t)1);
        const char *pid = real_get_property(real_dev, "ID_MODEL_ID");
        int is_dualsense = pid && !strcmp(pid, "0ce6");

        (void)fprintf(stderr, "[udev-spoof] Spoofing sysattr query on fake parent: %s\n", sysattr);

        if (!strcmp(sysattr, "manufacturer")) {
            return is_dualsense ? "Sony Interactive Entertainment" : "Sony Computer Entertainment";
        }
        if (!strcmp(sysattr, "product")) {
            return is_dualsense ? "DualSense Wireless Controller" : "Wireless Controller";
        }
        if (!strcmp(sysattr, "serial")) {
            return virtual_mac_serial();
        }
        if (!strcmp(sysattr, "uevent")) {
            const char *real_uevent = real_get_sysattr ? real_get_sysattr(real_dev, sysattr) : NULL;
            char *spoofed_uevent = next_uevent_buffer();
            (void)snprintf(spoofed_uevent, SPOOF_UEVENT_SIZE, "%s\nPRODUCT=054c/05c4/0100\n",
                           real_uevent ? real_uevent : "");
            return spoofed_uevent;
        }
    }

    if (real_get_sysattr) {
        struct udev_device *clean_dev = (struct udev_device *)((uintptr_t)udev_device & ~(uintptr_t)1);
        return real_get_sysattr(clean_dev, sysattr);
    }
    return NULL;
}
