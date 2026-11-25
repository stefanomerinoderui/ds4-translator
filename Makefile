# Prefer git's own description of the checked-out commit (exact tag, or
# <tag>-<n>-g<hash>[-dirty] between tags) since it's already the source of
# truth the release workflow tags from. Falls back to a VERSION file for
# builds without a .git directory — the release tarball ships one (written
# by the workflow from the tag it's building) so `make` still embeds a
# real version if a user extracts and rebuilds it themselves.
VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || cat VERSION 2>/dev/null || echo unknown)

CXX = g++
CC  = gcc

# Whatever compiler/linker flags the caller's environment set (e.g. makepkg's
# hardening flags), captured before this file overrides them below. make
# exports an overridden environment variable to every child process, so the
# UI's CMake build would otherwise inherit the daemon's -Werror set and fail
# on Qt's generated code, which that set is deliberately kept away from.
ENV_CXXFLAGS := $(CXXFLAGS)
ENV_LDFLAGS  := $(LDFLAGS)

PREFIX    = /usr/local
BINDIR    = $(PREFIX)/bin
SYSTEMDDIR = /etc/systemd/system
UDEVRULESDIR = /etc/udev/rules.d
MODPROBEDIR  = /etc/modprobe.d

# This project talks to the kernel through raw ioctls, configfs, and manual
# struct-packed HID reports -- exactly the code where an unnoticed implicit
# conversion, shadowed variable, or format-string slip turns into a silent
# low-level bug (or a security issue) instead of a compile error. Errors,
# not just warnings: `make` refuses to produce a binary until these are
# clean, rather than letting them quietly ship.
# -Wno-missing-field-initializers: this codebase's designated-initializer
# structs (FunctionFSDevice) deliberately list only the fields
# that need a non-zero starting value -- the C/C++ standard guarantees
# every omitted field is zero/false/null-initialized, so warning on that is
# a false positive here, not a real omission.
STRICT_WARNINGS = -Wall -Wextra -Werror -Wshadow -Wformat=2 -Wformat-security \
                   -Wnull-dereference -Wpointer-arith -Wcast-align -Wundef -Wwrite-strings \
                   -Wno-missing-field-initializers

BUILD_DIR = build
UI_BUILD_DIR = ui/build
UI_BINARY    = $(UI_BUILD_DIR)/ds4-translator-ui
# Extra CMake configure flags for the UI (the Arch package passes
# -DCMAKE_SKIP_RPATH=ON, since it ships the build-tree binary as is).
UI_CMAKE_FLAGS =

# Full HID-driver unbind + libusb raw-interrupt transport (see
# src/usb-hid-transport.* and src/hid-unbind-detect.*), the hide method
# that leaves no hidraw/input node for the physical controller at all
# (USB: fully readable via libusb; Bluetooth: driver unbind only, no live
# translation while hidden -- see set-hide-method). No longer gated behind
# a separate build flag: it's part of every build, same as the legacy
# chmod/setfacl/EVIOCGRAB method, and is the default hide_method.
ifeq ($(shell pkg-config --exists libusb-1.0 && echo yes),)
  $(error libusb-1.0 development package required (pkg-config libusb-1.0 not found))
endif
UNBIND_CXXFLAGS := $(shell pkg-config --cflags libusb-1.0)
UNBIND_LDFLAGS  := $(shell pkg-config --libs libusb-1.0)
UNBIND_SRC      := src/hid-unbind-detect.cpp src/usb-hid-transport.cpp
UNBIND_OBJ      := $(BUILD_DIR)/hid-unbind-detect.o $(BUILD_DIR)/usb-hid-transport.o

# HID-BPF replacement transport for the unbind hide method's Bluetooth
# side (see src/hid-bpf-transport.h and src/hid-bpf/hid-bpf-transport.bpf.c)
# -- USB's libusb transport above has no Bluetooth equivalent, so this
# fills that gap instead of leaving Bluetooth unbind-hidden-but-unreadable.
# Same dependency tier as libusb-1.0: hard build requirement, not optional.
ifeq ($(shell pkg-config --exists libbpf && echo yes),)
  $(error libbpf development package required (pkg-config libbpf not found))
endif
ifeq ($(shell command -v bpftool 2>/dev/null),)
  $(error bpftool not found (needed to generate vmlinux.h for the HID-BPF transport) -- try the "bpf" package)
endif
ifeq ($(shell command -v clang 2>/dev/null),)
  $(error clang not found (needed to compile src/hid-bpf/hid-bpf-transport.bpf.c))
endif
HIDBPF_CXXFLAGS := $(shell pkg-config --cflags libbpf)
HIDBPF_LDFLAGS  := $(shell pkg-config --libs libbpf)
HIDBPF_SRC      := src/hid-bpf-transport.cpp
HIDBPF_OBJ      := $(BUILD_DIR)/hid-bpf-transport.o
HIDBPF_BPF_OBJ  := $(BUILD_DIR)/hid-bpf-transport.bpf.o
HIDBPF_INSTALL_PATH := $(PREFIX)/lib/ds4-translator/hid-bpf-transport.bpf.o

# Hardening the build asks for explicitly, because whether a given toolchain
# applies it by default varies (Ubuntu's gcc does, Arch's only partly).
# -U first: some toolchains already define _FORTIFY_SOURCE, and a redefinition
# is a warning, which STRICT_WARNINGS turns into an error. FORTIFY needs
# optimisation, so the -O0 debug build leaves it out.
HARDEN_COMMON      = -fstack-protector-strong -fstack-clash-protection -fcf-protection
HARDEN_FLAGS       = $(HARDEN_COMMON) -fPIE
HARDEN_FORTIFY     = -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
HARDEN_LDFLAGS     = -pie -Wl,-z,relro,-z,now
HARDEN_SHARED_LDFLAGS = -Wl,-z,relro,-z,now

CXXFLAGS = -O3 $(STRICT_WARNINGS) $(HARDEN_FLAGS) $(HARDEN_FORTIFY) -std=c++17 -DDS4_VERSION=\"$(VERSION)\" \
           -DHID_BPF_OBJ_PATH=\"$(HIDBPF_INSTALL_PATH)\" $(UNBIND_CXXFLAGS) $(HIDBPF_CXXFLAGS)
CFLAGS   = -O3 $(STRICT_WARNINGS) $(HARDEN_FLAGS) $(HARDEN_FORTIFY) -DDS4_VERSION=\"$(VERSION)\" $(UNBIND_CXXFLAGS)
LDFLAGS  = -lpthread $(HARDEN_LDFLAGS) $(UNBIND_LDFLAGS) $(HIDBPF_LDFLAGS) $(ENV_LDFLAGS)

TARGET_DAEMON = $(BUILD_DIR)/ds4-translator
TARGET_CTL    = $(BUILD_DIR)/ds4-ctl
TARGET_SPOOF  = $(BUILD_DIR)/libudev-sony-spoof.so
TARGET_SPOOF32 = $(BUILD_DIR)/libudev-sony-spoof32.so

DAEMON_SRC = src/main.cpp src/translate.cpp src/fs-util.cpp src/functionfs-backend.c $(UNBIND_SRC) $(IPC_SRC)
# The control protocol itself: the client transport/command rules used by
# ds4-ctl and the Qt UI in ui/ (which compiles src/ipc-client.cpp into its
# own binary), plus the live-monitor STATE line format, which the daemon
# writes and both front-ends parse -- so the daemon links it too.
IPC_SRC    = src/ipc-client.cpp
CTL_SRC    = src/ctl.cpp $(IPC_SRC)
SPOOF_SRC  = src/udev-spoof.c

DAEMON_OBJ = $(BUILD_DIR)/main.o $(BUILD_DIR)/translate.o $(BUILD_DIR)/fs-util.o $(BUILD_DIR)/functionfs-backend.o $(UNBIND_OBJ) $(HIDBPF_OBJ) \
             $(BUILD_DIR)/ipc-client.o
CTL_OBJ    = $(BUILD_DIR)/ctl.o $(BUILD_DIR)/ipc-client.o

all: $(TARGET_DAEMON) $(TARGET_CTL) $(TARGET_SPOOF) $(TARGET_SPOOF32) $(HIDBPF_BPF_OBJ)

# Debug build: no optimisation, debug symbols, DS4_DEBUG enabled
debug: CXXFLAGS = -O0 -g $(STRICT_WARNINGS) $(HARDEN_FLAGS) -std=c++17 -DDS4_DEBUG -DDS4_VERSION=\"$(VERSION)\" \
                  -DHID_BPF_OBJ_PATH=\"$(HIDBPF_INSTALL_PATH)\" $(UNBIND_CXXFLAGS) $(HIDBPF_CXXFLAGS)
debug: CFLAGS   = -O0 -g $(STRICT_WARNINGS) $(HARDEN_FLAGS) -DDS4_DEBUG -DDS4_VERSION=\"$(VERSION)\" $(UNBIND_CXXFLAGS)
debug: clean all

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET_DAEMON): $(DAEMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_CTL): $(CTL_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_SPOOF): $(SPOOF_SRC) | $(BUILD_DIR)
	$(CC) -O3 -fPIC -shared $(HARDEN_COMMON) $(HARDEN_FORTIFY) $(HARDEN_SHARED_LDFLAGS) $(ENV_LDFLAGS) -o $@ $< -ldl -lpthread

$(TARGET_SPOOF32): $(SPOOF_SRC) | $(BUILD_DIR)
	$(CC) -m32 -O3 -fPIC -shared $(HARDEN_COMMON) $(HARDEN_FORTIFY) $(HARDEN_SHARED_LDFLAGS) $(ENV_LDFLAGS) -o $@ $< -ldl -lpthread


$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Machine-generated (kernel BTF -> C types) for the HID-BPF transport's
# CO-RE relocations -- deliberately not tracked in git (huge, and only
# needed at build time; libbpf resolves actual field offsets against
# whatever kernel BTF is present on the machine that *loads* the
# program, not this one, so building here and running elsewhere is
# fine as long as that machine also exposes /sys/kernel/btf/vmlinux).
$(BUILD_DIR)/vmlinux.h: | $(BUILD_DIR)
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > $@

# clang (not $(CC)/$(CXX)) targeting the BPF backend directly -- this is
# not a native object file. -D__TARGET_ARCH_x86 hardcodes this project's
# only tested/supported arch, same as the rest of the build.
$(HIDBPF_BPF_OBJ): src/hid-bpf/hid-bpf-transport.bpf.c src/hid-bpf/hid_bpf.h \
                   src/hid-bpf/hid_bpf_helpers.h src/hid-bpf/hid_report_descriptor_helpers.h \
                   $(BUILD_DIR)/vmlinux.h | $(BUILD_DIR)
	clang -O2 -g -target bpf -D__TARGET_ARCH_x86 \
	    -I $(BUILD_DIR) -I src/hid-bpf -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(UI_BUILD_DIR)

# Qt 6 settings GUI (ui/). Deliberately kept out of `all`: it needs Qt 6 +
# CMake, which the daemon itself does not, so a plain `make` still builds the
# daemon on a machine without them. `install` does include it -- use
# `install-cli` there instead.
# Always delegated: CMake already does its own up-to-date checking, so
# there is nothing for make to track here.
ui:
	CXXFLAGS='$(ENV_CXXFLAGS)' LDFLAGS='$(ENV_LDFLAGS)' \
	    cmake -S ui -B $(UI_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release $(UI_CMAKE_FLAGS)
	CXXFLAGS='$(ENV_CXXFLAGS)' LDFLAGS='$(ENV_LDFLAGS)' \
	    cmake --build $(UI_BUILD_DIR) -j$(shell nproc)

# The .desktop file is not cosmetic: the desktop portal looks the app up by
# it, and without it every launch logs "App info not found for
# 'ds4-translator-ui'". No autostart entry is installed: the tray applet is
# off by default and the settings window writes ~/.config/autostart itself
# when the user turns "Start the applet on login" on.
install-ui: stage-ui
ifdef SUDO_USER
	chown -R $(SUDO_USER):$(SUDO_USER) $(UI_BUILD_DIR)
endif

# File copies only, into $(DESTDIR) -- shared by install-ui and the Arch
# package's package() step, which must not touch the running system.
stage-ui: ui
	install -D -m 755 $(UI_BINARY) $(DESTDIR)$(BINDIR)/ds4-translator-ui
	install -D -m 644 ds4-translator-ui.desktop \
	    $(DESTDIR)/usr/share/applications/ds4-translator-ui.desktop

uninstall-ui:
	rm -f $(DESTDIR)$(BINDIR)/ds4-translator-ui
	rm -f $(DESTDIR)/usr/share/applications/ds4-translator-ui.desktop
	# Older versions installed this; removed here so an upgrade+uninstall
	# does not leave a system-wide autostart entry behind.
	rm -f $(DESTDIR)/etc/xdg/autostart/ds4-translator-applet.desktop

# Same thing for the current user only -- no root needed.
USER_BINDIR      = $(HOME)/.local/bin
USER_APPDIR      = $(HOME)/.local/share/applications
USER_AUTOSTART   = $(HOME)/.config/autostart

install-ui-user: ui
	install -D -m 755 $(UI_BINARY) $(USER_BINDIR)/ds4-translator-ui
	install -D -m 644 ds4-translator-ui.desktop \
	    $(USER_APPDIR)/ds4-translator-ui.desktop
	update-desktop-database $(USER_APPDIR) 2>/dev/null || true

uninstall-ui-user:
	rm -f $(USER_BINDIR)/ds4-translator-ui
	rm -f $(USER_APPDIR)/ds4-translator-ui.desktop
	rm -f $(USER_AUTOSTART)/ds4-translator-applet.desktop
	update-desktop-database $(USER_APPDIR) 2>/dev/null || true

# Everything: daemon, CLI and the Qt settings UI. `install-cli` below is the
# same thing without the UI, for machines with no Qt 6 / CMake (the UI is the
# only part that needs them) or where a GUI has no place at all.
install: install-cli install-ui

install-cli: stage-cli
	systemd-sysusers /usr/lib/sysusers.d/ds4-translator.conf
	udevadm control --reload-rules
	udevadm trigger
	systemctl daemon-reload
	systemctl enable ds4-translator.service
	systemctl restart ds4-translator.service
ifdef SUDO_USER
	chown -R $(SUDO_USER):$(SUDO_USER) $(BUILD_DIR)
endif

# File copies only, into $(DESTDIR) -- see stage-ui. The unit file hardcodes
# /usr/local/bin, so it is rewritten for whatever $(BINDIR) is in effect.
stage-cli: all
	install -D -m 755 $(TARGET_DAEMON) $(DESTDIR)$(BINDIR)/$(notdir $(TARGET_DAEMON))
	install -D -m 755 $(TARGET_CTL) $(DESTDIR)$(BINDIR)/$(notdir $(TARGET_CTL))
	install -D -m 755 $(TARGET_SPOOF) $(DESTDIR)/usr/lib/$(notdir $(TARGET_SPOOF))
	install -D -m 755 $(TARGET_SPOOF32) $(DESTDIR)/usr/lib32/$(notdir $(TARGET_SPOOF32))
	install -D -m 755 rebind-unbound-controllers.sh $(DESTDIR)$(BINDIR)/rebind-unbound-controllers.sh
	install -d $(DESTDIR)$(SYSTEMDDIR)
	sed 's|/usr/local/bin|$(BINDIR)|g' ds4-translator.service > $(DESTDIR)$(SYSTEMDDIR)/ds4-translator.service
	chmod 644 $(DESTDIR)$(SYSTEMDDIR)/ds4-translator.service
	install -D -m 644 72-ds4-translator-hide.rules $(DESTDIR)$(UDEVRULESDIR)/72-ds4-translator-hide.rules
	install -D -m 644 ds4-ctl.1 $(DESTDIR)/usr/share/man/man1/ds4-ctl.1
	install -D -m 644 ds4-ctl-completion.bash $(DESTDIR)/usr/share/bash-completion/completions/ds4-ctl
	install -D -m 644 ds4-ctl.fish $(DESTDIR)/usr/share/fish/vendor_completions.d/ds4-ctl.fish
	install -D -m 644 _ds4-ctl $(DESTDIR)/usr/share/zsh/site-functions/_ds4-ctl
	install -D -m 644 dummy_hcd.conf $(DESTDIR)$(MODPROBEDIR)/dummy_hcd.conf
	install -D -m 644 ds4-translator.sysusers $(DESTDIR)/usr/lib/sysusers.d/ds4-translator.conf
	install -D -m 644 $(HIDBPF_BPF_OBJ) $(DESTDIR)$(HIDBPF_INSTALL_PATH)

# Installable Arch Linux package(s), built by makepkg from a snapshot of the
# working tree (uncommitted changes included, so what you test is what you
# package). Produces two split packages in $(ARCH_PKG_DIR): ds4-translator
# (daemon + CLI) and ds4-translator-ui. Install with
#   sudo pacman -U build/arch/ds4-translator-*.pkg.tar.zst
# Must run as a regular user -- makepkg refuses to run as root.
ARCH_PKG_DIR = $(BUILD_DIR)/arch
# pacman versions can't contain '-': v0.6.0-7-g32cad17-dirty -> 0.6.0.r7.g32cad17.dirty
ARCH_PKGVER := $(shell echo '$(VERSION)' | sed -e 's/^v//' -e 's/-\([0-9]*\)-g/.r\1.g/' -e 's/-/./g')

# Optional GPG key (fingerprint or ID) to sign the packages and the repository
# database with; unsigned when empty. Needs the key in the caller's keyring.
ARCH_SIGN_KEY ?=
ARCH_SIGN_FLAGS = $(if $(ARCH_SIGN_KEY),--sign --key $(ARCH_SIGN_KEY))

arch-pkg:
	@if [ "$$(id -u)" -eq 0 ]; then echo "arch-pkg: run as a regular user, not root (makepkg refuses root)." >&2; exit 1; fi
	@command -v makepkg >/dev/null || { echo "arch-pkg: makepkg not found -- this target needs an Arch-based system." >&2; exit 1; }
	rm -rf $(ARCH_PKG_DIR)
	mkdir -p $(ARCH_PKG_DIR)/ds4-translator-$(ARCH_PKGVER)
	git ls-files -co --exclude-standard -z | grep -zv '^\.claude/' | \
	    xargs -0 cp --parents -t $(ARCH_PKG_DIR)/ds4-translator-$(ARCH_PKGVER)
	echo '$(VERSION)' > $(ARCH_PKG_DIR)/ds4-translator-$(ARCH_PKGVER)/VERSION
	tar -czf $(ARCH_PKG_DIR)/ds4-translator-$(ARCH_PKGVER).tar.gz \
	    -C $(ARCH_PKG_DIR) ds4-translator-$(ARCH_PKGVER)
	rm -rf $(ARCH_PKG_DIR)/ds4-translator-$(ARCH_PKGVER)
	sed 's|@PKGVER@|$(ARCH_PKGVER)|' packaging/arch/PKGBUILD.in > $(ARCH_PKG_DIR)/PKGBUILD
	cp packaging/arch/ds4-translator.install $(ARCH_PKG_DIR)/
	cd $(ARCH_PKG_DIR) && makepkg -f --cleanbuild $(ARCH_SIGN_FLAGS)
	@echo
	@echo "Packages built in $(ARCH_PKG_DIR):"
	@ls -1 $(ARCH_PKG_DIR)/*.pkg.tar.*
	@echo "Install with: sudo pacman -U $(ARCH_PKG_DIR)/ds4-translator-*.pkg.tar.zst"

# Turns the packages `arch-pkg` built into a pacman repository named
# "ds4-translator" in the same directory (see packaging/arch/README.md).
# repo-add leaves ds4-translator.db/.files as symlinks to the .tar.zst
# files; static hosts such as GitHub release assets can't serve symlinks, so
# they are replaced with real copies.
arch-repo:
	@command -v repo-add >/dev/null || { echo "arch-repo: repo-add not found -- this target needs pacman's tools." >&2; exit 1; }
	@ls $(ARCH_PKG_DIR)/*.pkg.tar.zst >/dev/null 2>&1 || { echo "arch-repo: no packages in $(ARCH_PKG_DIR) -- run 'make arch-pkg' first." >&2; exit 1; }
	cd $(ARCH_PKG_DIR) && rm -f ds4-translator.db* ds4-translator.files* && \
	    repo-add $(ARCH_SIGN_FLAGS) ds4-translator.db.tar.zst *.pkg.tar.zst && \
	    rm ds4-translator.db ds4-translator.files && \
	    cp ds4-translator.db.tar.zst ds4-translator.db && \
	    cp ds4-translator.files.tar.zst ds4-translator.files
	if [ -n "$(ARCH_SIGN_KEY)" ]; then \
	    cd $(ARCH_PKG_DIR) && rm -f ds4-translator.db.sig ds4-translator.files.sig && \
	    cp ds4-translator.db.tar.zst.sig ds4-translator.db.sig && \
	    cp ds4-translator.files.tar.zst.sig ds4-translator.files.sig; \
	fi


# ------------------------------------------------------------------- tests
# GoogleTest suites under tests/. Everything here runs unprivileged on a
# machine with no controller attached (CI runners included): the suites cover
# the pure logic and the IPC/CLI surface, never real hardware. They are built
# with the same STRICT_WARNINGS as the product so test code is linted too.
TEST_BUILD_DIR = $(BUILD_DIR)/tests
GTEST_CXXFLAGS := $(shell pkg-config --cflags gtest 2>/dev/null)
GTEST_LDFLAGS  := $(shell pkg-config --libs gtest_main 2>/dev/null || echo -lgtest -lgtest_main)
TEST_CXXFLAGS  = -O1 -g $(STRICT_WARNINGS) -std=c++17 -DDS4_VERSION=\"test\" -Isrc $(GTEST_CXXFLAGS)
# Narrow the run to matching test names, e.g. GTEST_FILTER='*Sanity*'.
GTEST_FILTER ?= *
GTEST_ARGS    = --gtest_filter='$(GTEST_FILTER)' --gtest_output=xml:$(TEST_BUILD_DIR)/$(notdir $@).xml

UNIT_TEST_BIN = $(TEST_BUILD_DIR)/unit-tests
UNIT_TEST_SRC = $(wildcard tests/unit/*.cpp)
# hid-unbind-detect.cpp needs write_hid_driver_sysfs from main.cpp; the stub
# stands in so it links without the daemon.
UNIT_LIB_SRC  = src/ipc-client.cpp src/translate.cpp src/fs-util.cpp src/hid-unbind-detect.cpp tests/stubs/hid_driver_sysfs_stub.cpp

$(UNIT_TEST_BIN): $(UNIT_TEST_SRC) $(UNIT_LIB_SRC) $(wildcard src/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -o $@ $(UNIT_TEST_SRC) $(UNIT_LIB_SRC) $(GTEST_LDFLAGS) -lpthread

test-unit: $(UNIT_TEST_BIN)
	$(UNIT_TEST_BIN) $(GTEST_ARGS)

# Fake-libusb transport tests; see the rules further down.
USB_TEST_SRC = tests/component/test_usb_transport.cpp tests/stubs/libusb_stub.cpp \
               src/usb-hid-transport.cpp tests/stubs/hid_driver_sysfs_stub.cpp
USB_TEST_BIN = $(TEST_BUILD_DIR)/usb-transport-tests

# Same unit suites under ASan+UBSan: memory/UB bugs in the parsers fail the run.
SAN_TEST_BIN = $(TEST_BUILD_DIR)/unit-tests-san
$(SAN_TEST_BIN): $(UNIT_TEST_SRC) $(UNIT_LIB_SRC) $(wildcard src/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined \
	    -fno-omit-frame-pointer -o $@ $(UNIT_TEST_SRC) $(UNIT_LIB_SRC) $(GTEST_LDFLAGS) -lpthread

test-sanitize: $(SAN_TEST_BIN) $(USB_TEST_BIN)-san
	$(SAN_TEST_BIN) $(GTEST_ARGS)
	$(USB_TEST_BIN)-san $(GTEST_ARGS)

COMPONENT_TEST_BIN = $(TEST_BUILD_DIR)/component-tests
COMPONENT_TEST_SRC = tests/component/test_ipc_transport.cpp
$(COMPONENT_TEST_BIN): $(COMPONENT_TEST_SRC) src/ipc-client.cpp $(wildcard src/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -o $@ $(COMPONENT_TEST_SRC) src/ipc-client.cpp $(GTEST_LDFLAGS) -lpthread

# udev-spoof.so against a stub libudev (see tests/stubs/libudev_stub.c): the
# shim dlopen()s libudev.so.1, so the stub is put first on LD_LIBRARY_PATH.
SPOOF_STUB_DIR  = $(TEST_BUILD_DIR)/udev-stub
SPOOF_TEST_BIN  = $(TEST_BUILD_DIR)/spoof-tests
$(SPOOF_STUB_DIR)/libudev.so.1: tests/stubs/libudev_stub.c
	@mkdir -p $(SPOOF_STUB_DIR)
	$(CC) -fPIC -shared -Wl,-soname,libudev.so.1 -o $@ $<

$(SPOOF_TEST_BIN): tests/component/test_udev_spoof.cpp $(TARGET_SPOOF)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -o $@ tests/component/test_udev_spoof.cpp \
	    -L$(BUILD_DIR) -l:libudev-sony-spoof.so -Wl,-rpath,$(abspath $(BUILD_DIR)) $(GTEST_LDFLAGS) -lpthread

# usb-hid-transport.cpp against a fake libusb (tests/stubs/libusb_stub.cpp)
# instead of -lusb-1.0. Built three ways: plain, ASan+UBSan and TSan, since the
# bug class it guards (M3: closing a handle under an in-flight transfer) is a
# memory/thread-safety one.
$(USB_TEST_BIN): $(USB_TEST_SRC) $(wildcard src/*.h tests/stubs/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -o $@ $(USB_TEST_SRC) $(GTEST_LDFLAGS) -lpthread
$(USB_TEST_BIN)-san: $(USB_TEST_SRC) $(wildcard src/*.h tests/stubs/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined \
	    -fno-omit-frame-pointer -o $@ $(USB_TEST_SRC) $(GTEST_LDFLAGS) -lpthread
$(USB_TEST_BIN)-tsan: $(USB_TEST_SRC) $(wildcard src/*.h tests/stubs/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -fsanitize=thread -o $@ $(USB_TEST_SRC) $(GTEST_LDFLAGS) -lpthread

test-component: $(COMPONENT_TEST_BIN) $(SPOOF_TEST_BIN) $(SPOOF_STUB_DIR)/libudev.so.1 $(USB_TEST_BIN)
	$(COMPONENT_TEST_BIN) $(GTEST_ARGS)
	LD_LIBRARY_PATH=$(SPOOF_STUB_DIR) $(SPOOF_TEST_BIN) $(GTEST_ARGS)
	$(USB_TEST_BIN) $(GTEST_ARGS)

# Performance budgets on the per-report hot paths; prints measured ns/op.
PERF_TEST_BIN = $(TEST_BUILD_DIR)/perf-tests
$(PERF_TEST_BIN): tests/perf/bench_protocol.cpp src/ipc-client.cpp src/translate.cpp $(wildcard src/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -O2 -o $@ tests/perf/bench_protocol.cpp src/ipc-client.cpp src/translate.cpp $(GTEST_LDFLAGS) -lpthread

test-perf: $(PERF_TEST_BIN)
	$(PERF_TEST_BIN) $(GTEST_ARGS)

# libFuzzer over the text parsers and the uevent reader (needs clang).
# FUZZ_SECONDS bounds each target; crashing inputs are left in
# $(TEST_BUILD_DIR)/fuzz-crashes.
FUZZ_SECONDS ?= 30
FUZZ_TARGETS = parsers uevent
FUZZ_LIB_SRC = src/ipc-client.cpp src/hid-unbind-detect.cpp tests/stubs/hid_driver_sysfs_stub.cpp
$(TEST_BUILD_DIR)/fuzz-%: tests/fuzz/fuzz_%.cpp $(FUZZ_LIB_SRC) $(wildcard src/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	clang++ -g -O1 -std=c++17 -Isrc -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined \
	    -o $@ $< $(FUZZ_LIB_SRC)

test-fuzz: $(addprefix $(TEST_BUILD_DIR)/fuzz-,$(FUZZ_TARGETS))
	@mkdir -p $(TEST_BUILD_DIR)/fuzz-crashes
	@set -e; for t in $(FUZZ_TARGETS); do \
	    mkdir -p $(TEST_BUILD_DIR)/fuzz-corpus-$$t; \
	    echo "== fuzzing $$t for $(FUZZ_SECONDS)s"; \
	    $(TEST_BUILD_DIR)/fuzz-$$t -max_total_time=$(FUZZ_SECONDS) \
	        -artifact_prefix=$(TEST_BUILD_DIR)/fuzz-crashes/ $(TEST_BUILD_DIR)/fuzz-corpus-$$t; \
	done

# Unit + component transport suites under ThreadSanitizer: guards the shared
# client code against data races (the udev shim and daemon are covered by
# docs/security-findings.md instead).
TSAN_UNIT_BIN = $(TEST_BUILD_DIR)/unit-tests-tsan
$(TSAN_UNIT_BIN): $(UNIT_TEST_SRC) tests/component/test_ipc_transport.cpp $(UNIT_LIB_SRC) $(wildcard src/*.h)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(TEST_CXXFLAGS) -fsanitize=thread -o $@ $(UNIT_TEST_SRC) tests/component/test_ipc_transport.cpp \
	    $(UNIT_LIB_SRC) $(GTEST_LDFLAGS) -lpthread

test-tsan: $(TSAN_UNIT_BIN) $(USB_TEST_BIN)-tsan
	$(TSAN_UNIT_BIN) $(GTEST_ARGS)
	$(USB_TEST_BIN)-tsan $(GTEST_ARGS)

# Hardening flags, systemd unit, udev rules, workflows, shell scripts, secrets.
test-security-static: all
	tests/security/static_checks.sh $(BUILD_DIR)

test-smoke: all
	tests/smoke/smoke.sh $(BUILD_DIR)

# bash/fish/zsh completion scripts against the built ds4-ctl; a shell that is
# not installed is skipped.
test-completion: all
	tests/completion/test_completion.sh $(BUILD_DIR)

# Real daemon + ds4-ctl against a scratch dir (see paths.h); skips itself if a
# production daemon is running on this machine.
test-integration: all
	tests/integration/test_daemon_ctl.sh $(BUILD_DIR)
	tests/integration/test_daemon_security.sh $(BUILD_DIR)

# The hwtest runner (tools/hwtest) is Python: its tests need neither hardware
# nor root. Lint it with `ruff check tools/hwtest tests/hwtest`.
test-hwtest:
	python3 -m unittest discover -s tests/hwtest

test: test-unit test-component test-smoke test-completion test-integration test-security-static test-hwtest

# Mirrors install: removes the UI too. uninstall-cli leaves it in place.
uninstall: uninstall-ui uninstall-cli

uninstall-cli:
	systemctl disable --now ds4-translator.service || true
	rm -f $(DESTDIR)$(BINDIR)/$(notdir $(TARGET_DAEMON))
	rm -f $(DESTDIR)$(BINDIR)/$(notdir $(TARGET_CTL))
	rm -f $(DESTDIR)/usr/lib/$(notdir $(TARGET_SPOOF))
	rm -f $(DESTDIR)/usr/lib32/$(notdir $(TARGET_SPOOF32))
	rm -f $(DESTDIR)$(BINDIR)/rebind-unbound-controllers.sh
	rm -f $(DESTDIR)$(SYSTEMDDIR)/ds4-translator.service
	rm -f $(DESTDIR)$(UDEVRULESDIR)/72-ds4-translator-hide.rules
	rm -f $(DESTDIR)/usr/share/man/man1/ds4-ctl.1
	rm -f $(DESTDIR)/usr/share/bash-completion/completions/ds4-ctl
	rm -f $(DESTDIR)/usr/share/fish/vendor_completions.d/ds4-ctl.fish
	rm -f $(DESTDIR)/usr/share/zsh/site-functions/_ds4-ctl
	rm -f $(DESTDIR)$(MODPROBEDIR)/dummy_hcd.conf
	rm -f $(DESTDIR)/usr/lib/sysusers.d/ds4-translator.conf
	rm -f $(DESTDIR)$(HIDBPF_INSTALL_PATH)
	udevadm control --reload-rules
	udevadm trigger
	systemctl daemon-reload

.PHONY: all debug clean install install-cli stage-cli uninstall uninstall-cli \
        ui install-ui stage-ui uninstall-ui install-ui-user uninstall-ui-user arch-pkg arch-repo \
        test test-unit test-perf test-fuzz test-component test-smoke test-completion test-integration test-sanitize test-tsan test-security-static test-hwtest
