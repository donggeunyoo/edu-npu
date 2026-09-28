/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_EDU_NPU_H
#define _UAPI_EDU_NPU_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct edu_npu_fact {
	__u32 n;
	__u32 result;
};

#define EDU_NPU_IOC_MAGIC	0xed
#define EDU_NPU_IOC_FACT	_IOWR(EDU_NPU_IOC_MAGIC, 0x00, struct edu_npu_fact)

#endif
