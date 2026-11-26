# SPDX-License-Identifier: GPL-2.0-only
#
# Out-of-tree build of hid-ds4-translator. Usage:
#   make                      build against the running kernel
#   make KDIR=/path/to/build  build against another kernel tree
#   make strict               rebuild with W=1 and every warning an error
#   make check                checkpatch.pl --strict on the sources
#   make sparse               sparse's static checks, findings are errors
#   make analyze              Clang static analyzer, findings are errors
#   make lint                 all of the above (analyze only on a Clang kernel)
#
# Plain `make` is what DKMS runs on users' machines, with whatever compiler
# their kernel was built with, so it never turns warnings into errors: a new
# compiler's new warning must not stop a module from building. The strict
# targets are for development and CI.

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

# A module must be built with the toolchain that built the kernel (CachyOS
# kernels use Clang with LTO), or modpost/vermagic reject it.
ifneq ($(shell grep -s '^CONFIG_CC_IS_CLANG=y' $(KDIR)/.config),)
  KBUILD_ARGS += LLVM=1
endif

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) clean

# ALLOC_WITH_SIZEOF: kmalloc_obj() only exists in recent kernels, and the
# driver has to build back to 6.12.
# ENOSYS: hid_hw_output_report() returns -ENOSYS by contract when the
# transport has no output endpoint, and the write path tests for it (as
# hidraw does).
# Not every kernel's build tree ships checkpatch.pl; CHECKPATCH overrides it.
CHECKPATCH ?= $(KDIR)/scripts/checkpatch.pl
check:
	$(CHECKPATCH) --no-tree --strict \
		--ignore ALLOC_WITH_SIZEOF,ENOSYS \
		-f ds4t-driver.c -f ds4t-chardev.c -f ds4t-control.c \
		-f ds4t-vcore.c -f ds4t-vcore.h -f ds4t-vhid.c \
		-f ds4t-translate.c -f ds4t-translate.h \
		-f ds4t-descriptors.h -f ds4t-internal.h

# A clean build first, or Kbuild skips every file that is up to date and the
# warnings never show. W=2 and up report on the kernel's own headers, not on
# this code, so W=1 is the useful ceiling.
strict: clean
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) W=1 KCFLAGS=-Werror modules

# -Wsparse-error turns sparse's warnings into errors.
# Kbuild only warns when sparse is missing and then builds without checking, so
# its absence is made an error here.
sparse: clean
	@command -v sparse >/dev/null || { echo "sparse: not installed" >&2; exit 1; }
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) C=2 CF="-D__CHECK_ENDIAN__ -Wsparse-error" modules

# Kbuild runs analyze.sh on each source with the module's real compiler flags.
# Those flags are the kernel's, and a GCC-built kernel's contain options Clang
# does not know, so this needs a kernel built with Clang.
analyze: clean
	@grep -qs '^CONFIG_CC_IS_CLANG=y' $(KDIR)/.config || \
	    { echo "analyze: needs a kernel built with Clang ($(KDIR) was not)" >&2; exit 1; }
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) C=2 CHECK=$(CURDIR)/analyze.sh modules

lint: strict check
	@if command -v sparse >/dev/null; then $(MAKE) sparse; \
	else echo "lint: skipping sparse (not installed)"; fi
	@if grep -qs '^CONFIG_CC_IS_CLANG=y' $(KDIR)/.config; then $(MAKE) analyze; \
	else echo "lint: skipping analyze (this kernel was not built with Clang)"; fi

.PHONY: all clean check strict sparse analyze lint
