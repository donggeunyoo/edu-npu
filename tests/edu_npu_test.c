// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

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
	/* 13!은 32비트를 넘으므로 2^32로 나눈 나머지가 나온다 */
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

static struct kunit_case edu_npu_test_cases[] = {
	KUNIT_CASE_PARAM(edu_npu_test_factorial, edu_npu_fact_gen_params),
	{ }
};

static struct kunit_suite edu_npu_test_suite = {
	.name = "edu_npu",
	.init = edu_npu_test_init,
	.exit = edu_npu_test_exit,
	.test_cases = edu_npu_test_cases,
};
kunit_test_suite(edu_npu_test_suite);
