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

static void edu_npu_test_factorial_5(struct kunit *test)
{
	struct edu_npu_test_ctx *ctx = test->priv;
	u32 result;

	KUNIT_ASSERT_EQ(test, edu_npu_factorial(ctx->regs, 5, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 120);
}

static struct kunit_case edu_npu_test_cases[] = {
	KUNIT_CASE(edu_npu_test_factorial_5),
	{ }
};

static struct kunit_suite edu_npu_test_suite = {
	.name = "edu_npu",
	.init = edu_npu_test_init,
	.exit = edu_npu_test_exit,
	.test_cases = edu_npu_test_cases,
};
kunit_test_suite(edu_npu_test_suite);
