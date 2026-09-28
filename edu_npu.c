// SPDX-License-Identifier: GPL-2.0

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/dev_printk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/types.h>

#define EDU_PCI_VENDOR_ID	0x1234
#define EDU_PCI_DEVICE_ID	0x11e8

#define EDU_REG_ID		0x00
#define EDU_ID_MAJOR		GENMASK(31, 24)
#define EDU_ID_MINOR		GENMASK(23, 16)
#define EDU_ID_MAGIC		GENMASK(7, 0)
#define EDU_ID_MAGIC_VAL	0xed
#define EDU_REG_FACT		0x08
#define EDU_REG_STATUS		0x20
#define EDU_STATUS_COMPUTING	BIT(0)

#define EDU_FACT_POLL_US	10
#define EDU_FACT_TIMEOUT_US	10000

struct edu_npu {
	void __iomem *regs;
	struct miscdevice misc;
};

static const struct pci_device_id edu_npu_ids[] = {
	{ PCI_DEVICE(EDU_PCI_VENDOR_ID, EDU_PCI_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, edu_npu_ids);

static int edu_npu_wait_idle(void __iomem *regs)
{
	u32 status;

	return read_poll_timeout(ioread32, status,
				 !(status & EDU_STATUS_COMPUTING),
				 EDU_FACT_POLL_US, EDU_FACT_TIMEOUT_US, false,
				 regs + EDU_REG_STATUS);
}

static int edu_npu_factorial(void __iomem *regs, u32 n, u32 *result)
{
	int ret;

	ret = edu_npu_wait_idle(regs);
	if (ret)
		return ret;

	iowrite32(n, regs + EDU_REG_FACT);

	ret = edu_npu_wait_idle(regs);
	if (ret)
		return ret;

	*result = ioread32(regs + EDU_REG_FACT);
	return 0;
}

static const struct file_operations edu_npu_fops = {
	.owner = THIS_MODULE,
};

static void edu_npu_misc_deregister(void *data)
{
	misc_deregister(data);
}

static int edu_npu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_npu *npu;
	u32 ident, fact;
	int ret;

	npu = devm_kzalloc(&pdev->dev, sizeof(*npu), GFP_KERNEL);
	if (!npu)
		return -ENOMEM;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	npu->regs = pcim_iomap_region(pdev, 0, KBUILD_MODNAME);
	if (IS_ERR(npu->regs))
		return PTR_ERR(npu->regs);

	ident = ioread32(npu->regs + EDU_REG_ID);
	if (FIELD_GET(EDU_ID_MAGIC, ident) != EDU_ID_MAGIC_VAL)
		return -ENODEV;

	dev_info(&pdev->dev, "edu v%lu.%lu\n",
		 FIELD_GET(EDU_ID_MAJOR, ident), FIELD_GET(EDU_ID_MINOR, ident));

	ret = edu_npu_factorial(npu->regs, 5, &fact);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "5! = %u\n", fact);

	npu->misc.minor = MISC_DYNAMIC_MINOR;
	npu->misc.name = KBUILD_MODNAME;
	npu->misc.fops = &edu_npu_fops;
	npu->misc.parent = &pdev->dev;

	ret = misc_register(&npu->misc);
	if (ret)
		return ret;

	return devm_add_action_or_reset(&pdev->dev, edu_npu_misc_deregister,
					&npu->misc);
}

static struct pci_driver edu_npu_driver = {
	.name = KBUILD_MODNAME,
	.id_table = edu_npu_ids,
	.probe = edu_npu_probe,
};
module_pci_driver(edu_npu_driver);

MODULE_DESCRIPTION("Driver for the QEMU edu PCI device");
MODULE_LICENSE("GPL");

#if IS_ENABLED(CONFIG_KUNIT)
#include "tests/edu_npu_test.c"
#endif
