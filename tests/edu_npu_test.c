// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#define EDU_NPU_TEST_BUSY_N	1000000

struct edu_npu_test_ctx {
	struct pci_dev *pdev;
	void __iomem *regs;
};

static int edu_npu_test_init(struct kunit *test)
{
	struct edu_npu_test_ctx *ctx;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->pdev = pci_get_device(EDU_PCI_VENDOR_ID, EDU_PCI_DEVICE_ID, NULL);
	if (!ctx->pdev)
		return -ENODEV;

	ctx->regs = pci_iomap(ctx->pdev, 0, 0);
	if (!ctx->regs)
		return -ENOMEM;

	return 0;
}

static void edu_npu_test_exit(struct kunit *test)
{
	struct edu_npu_test_ctx *ctx = test->priv;

	if (!ctx)
		return;
	if (ctx->regs)
		pci_iounmap(ctx->pdev, ctx->regs);
	pci_dev_put(ctx->pdev);
}

struct edu_npu_fact_case {
	u32 n;
	u32 expected;
};

static const struct edu_npu_fact_case edu_npu_fact_cases[] = {
	{ .n = 0, .expected = 1 },
	{ .n = 1, .expected = 1 },
	{ .n = 5, .expected = 120 },
	{ .n = 12, .expected = 479001600 },
	{ .n = 13, .expected = 1932053504 },
};

static void edu_npu_fact_case_desc(const struct edu_npu_fact_case *c,
				   char *desc)
{
	snprintf(desc, KUNIT_PARAM_DESC_SIZE, "%u!", c->n);
}

KUNIT_ARRAY_PARAM(edu_npu_fact, edu_npu_fact_cases, edu_npu_fact_case_desc);

static void edu_npu_test_factorial(struct kunit *test)
{
	const struct edu_npu_fact_case *c = test->param_value;
	struct edu_npu_test_ctx *ctx = test->priv;
	u32 result;

	KUNIT_ASSERT_EQ(test, edu_npu_factorial(ctx->regs, c->n, &result), 0);
	KUNIT_EXPECT_EQ(test, result, c->expected);
}

static void edu_npu_test_factorial_busy(struct kunit *test)
{
	struct edu_npu_test_ctx *ctx = test->priv;
	u32 result;

	iowrite32(EDU_NPU_TEST_BUSY_N, ctx->regs + EDU_REG_FACT);
	KUNIT_ASSERT_TRUE(test, ioread32(ctx->regs + EDU_REG_STATUS) &
			  EDU_STATUS_COMPUTING);

	KUNIT_ASSERT_EQ(test, edu_npu_factorial(ctx->regs, 5, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 120);
}

static struct kunit_case edu_npu_test_cases[] = {
	KUNIT_CASE_PARAM(edu_npu_test_factorial, edu_npu_fact_gen_params),
	KUNIT_CASE(edu_npu_test_factorial_busy),
	{ }
};

static struct kunit_suite edu_npu_test_suite = {
	.name = "edu_npu",
	.init = edu_npu_test_init,
	.exit = edu_npu_test_exit,
	.test_cases = edu_npu_test_cases,
};
static void edu_npu_test_factorial_timeout(struct kunit *test)
{
	void __iomem *regs;
	u32 *fake, result = 0xdeadbeef;

	fake = kunit_kzalloc(test, EDU_REG_STATUS + sizeof(u32), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, fake);
	fake[EDU_REG_STATUS / sizeof(u32)] = EDU_STATUS_COMPUTING;
	regs = (void __force __iomem *)fake;

	KUNIT_EXPECT_EQ(test, edu_npu_factorial(regs, 5, &result), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, result, 0xdeadbeef);
	KUNIT_EXPECT_EQ(test, fake[EDU_REG_FACT / sizeof(u32)], 0);
}

static struct kunit_case edu_npu_fake_test_cases[] = {
	KUNIT_CASE(edu_npu_test_factorial_timeout),
	{ }
};

static struct kunit_suite edu_npu_fake_test_suite = {
	.name = "edu_npu_fake",
	.test_cases = edu_npu_fake_test_cases,
};

kunit_test_suites(&edu_npu_test_suite, &edu_npu_fake_test_suite);
