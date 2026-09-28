# SPDX-License-Identifier: GPL-2.0

ifneq ($(KERNELRELEASE),)

obj-m := edu_npu.o
ccflags-y := -Werror

else

KDIR = /lib/modules/$(shell uname -r)/build
KSFT_DIR = $(KDIR)/source/tools/testing/selftests

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f tools/edu-fact tools/edu-npu-test

tools/edu-fact: tools/edu-fact.c include/uapi/edu_npu.h
	$(CC) -static -O2 -Wall -Wextra -Werror -Iinclude/uapi -o $@ $<

tools/edu-npu-test: tools/edu-npu-test.c include/uapi/edu_npu.h
	$(CC) -static -O2 -Wall -Wextra -Werror -Iinclude/uapi -I$(KSFT_DIR) \
		-o $@ $<

initramfs: all tools/edu-fact tools/edu-npu-test
	rm -rf vm/rootfs
	mkdir -p vm/rootfs/bin vm/rootfs/dev vm/rootfs/proc vm/rootfs/sys
	cp /usr/bin/busybox tools/edu-fact tools/edu-npu-test vm/rootfs/bin/
	cp vm/init edu_npu.ko vm/rootfs/
	cd vm/rootfs && find . | cpio -o -H newc --quiet > ../initramfs.cpio

run: initramfs
	qemu-system-x86_64 -enable-kvm -cpu host -m 512M -nographic -no-reboot \
	-kernel vm/vmlinuz -initrd vm/initramfs.cpio \
	-append "console=ttyS0 panic=-1" \
	-device edu

.PHONY: all clean initramfs run

endif
