// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "edu_npu.h"
#include "kselftest_harness.h"

#define EDU_NPU_TEST_WORKERS	4
#define EDU_NPU_TEST_ROUNDS	2000

static const __u32 edu_npu_fact_table[] = {
	1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800, 39916800,
	479001600,
};

static int edu_npu_worker(int fd, int id)
{
	struct edu_npu_fact req;
	int round, wrong = 0;

	for (round = 0; round < EDU_NPU_TEST_ROUNDS; round++) {
		req.n = (id + round) % ARRAY_SIZE(edu_npu_fact_table);
		if (ioctl(fd, EDU_NPU_IOC_FACT, &req) ||
		    req.result != edu_npu_fact_table[req.n])
			wrong++;
	}

	if (wrong)
		fprintf(stderr, "# worker %d: %d/%d wrong\n", id, wrong,
			EDU_NPU_TEST_ROUNDS);
	return wrong ? 1 : 0;
}

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
	ASSERT_TRUE(req != MAP_FAILED);
	req->n = 5;
	ASSERT_EQ(0, mprotect(req, sizeof(*req), PROT_READ));

	ret = ioctl(self->fd, EDU_NPU_IOC_FACT, req);
	err = errno;
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EFAULT, err);
	EXPECT_EQ(0U, req->result);

	munmap(req, sizeof(*req));
}

TEST_F_TIMEOUT(edu_npu, timeout_then_recover, 90)
{
	struct edu_npu_fact req = { .n = UINT32_MAX };
	struct timespec start, end;
	int ret, err, tries;

	ret = ioctl(self->fd, EDU_NPU_IOC_FACT, &req);
	err = errno;
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ETIMEDOUT, err);

	clock_gettime(CLOCK_MONOTONIC, &start);
	req.n = 5;
	for (tries = 1; tries <= 600; tries++) {
		ret = ioctl(self->fd, EDU_NPU_IOC_FACT, &req);
		err = errno;
		if (ret == 0 || err != ETIMEDOUT)
			break;
		usleep(100000);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	TH_LOG("recovered after %d tries, %ld ms", tries,
	       (end.tv_sec - start.tv_sec) * 1000 +
	       (end.tv_nsec - start.tv_nsec) / 1000000);
	ASSERT_EQ(0, ret);
	EXPECT_EQ(120U, req.result);
}

TEST_F(edu_npu, concurrent_requests)
{
	pid_t pids[EDU_NPU_TEST_WORKERS];
	int i, status, failed = 0;

	for (i = 0; i < EDU_NPU_TEST_WORKERS; i++) {
		pids[i] = fork();
		ASSERT_LE(0, pids[i]);
		if (!pids[i])
			_exit(edu_npu_worker(self->fd, i));
	}

	for (i = 0; i < EDU_NPU_TEST_WORKERS; i++) {
		ASSERT_EQ(pids[i], waitpid(pids[i], &status, 0));
		ASSERT_TRUE(WIFEXITED(status));
		failed += WEXITSTATUS(status);
	}

	EXPECT_EQ(0, failed);
}

TEST_HARNESS_MAIN
