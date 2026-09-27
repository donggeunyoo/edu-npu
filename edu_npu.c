// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

static int __init edu_npu_init(void)
{
	pr_info("loaded\n");
	return 0;
}
module_init(edu_npu_init);

static void __exit edu_npu_exit(void)
{
	pr_info("unloaded\n");
}
module_exit(edu_npu_exit);

MODULE_DESCRIPTION("Driver for the QEMU edu PCI device");
MODULE_LICENSE("GPL");
