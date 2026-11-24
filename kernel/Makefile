# SPDX-License-Identifier: GPL-2.0-only
#
# Out-of-tree build of hid-ds4-translator. Usage:
#   make                      build against the running kernel
#   make KDIR=/path/to/build  build against another kernel tree
#   make check                checkpatch.pl --strict on the sources

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
check:
	$(KDIR)/scripts/checkpatch.pl --no-tree --strict \
		--ignore ALLOC_WITH_SIZEOF -f hid-ds4-translator.c

.PHONY: all clean check
