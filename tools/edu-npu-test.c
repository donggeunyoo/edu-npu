// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "edu_npu.h"
#include "kselftest_harness.h"

FIXTURE(edu_npu)
{
	int fd;
};

FIXTURE_SETUP(edu_npu)
{
	self->fd = open("/dev/edu_npu", O_RDWR);
	ASSERT_LE(0, self->fd);
}

FIXTURE_TEARDOWN(edu_npu)
{
	close(self->fd);
}

TEST_F(edu_npu, unknown_cmd)
{
	struct edu_npu_fact req = { .n = 5 };
	int ret, err;

	ret = ioctl(self->fd, _IO(EDU_NPU_IOC_MAGIC, 0x01), &req);
	err = errno;
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOTTY, err);
}

TEST_F(edu_npu, null_request)
{
	int ret, err;

	ret = ioctl(self->fd, EDU_NPU_IOC_FACT, NULL);
	err = errno;
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EFAULT, err);
}

TEST_F(edu_npu, readonly_request)
{
	struct edu_npu_fact *req;
	int ret, err;

	req = mmap(NULL, sizeof(*req), PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	ASSERT_NE(MAP_FAILED, req);
	req->n = 5;
	ASSERT_EQ(0, mprotect(req, sizeof(*req), PROT_READ));

	ret = ioctl(self->fd, EDU_NPU_IOC_FACT, req);
	err = errno;
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EFAULT, err);
	EXPECT_EQ(0U, req->result);

	munmap(req, sizeof(*req));
}

TEST_HARNESS_MAIN
