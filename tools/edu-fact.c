// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "edu_npu.h"

int main(int argc, char **argv)
{
	struct edu_npu_fact req = { 0 };
	unsigned long n;
	char *end;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s N\n", argv[0]);
		return 2;
	}

	errno = 0;
	n = strtoul(argv[1], &end, 0);
	if (errno || end == argv[1] || *end || n > UINT32_MAX) {
		fprintf(stderr, "invalid N: %s\n", argv[1]);
		return 2;
	}
	req.n = n;

	fd = open("/dev/edu_npu", O_RDWR);
	if (fd < 0) {
		perror("/dev/edu_npu");
		return 1;
	}

	if (ioctl(fd, EDU_NPU_IOC_FACT, &req) < 0) {
		perror("EDU_NPU_IOC_FACT");
		close(fd);
		return 1;
	}

	printf("%u! = %u\n", req.n, req.result);
	close(fd);
	return 0;
}
