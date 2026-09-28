// SPDX-License-Identifier: GPL-2.0

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/dev_printk.h>
#include <linux/err.h>
#include <linux/io.h>
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

static const struct pci_device_id edu_npu_ids[] = {
	{ PCI_DEVICE(EDU_PCI_VENDOR_ID, EDU_PCI_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, edu_npu_ids);

static int edu_npu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	void __iomem *regs;
	u32 ident;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	regs = pcim_iomap_region(pdev, 0, KBUILD_MODNAME);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	ident = ioread32(regs + EDU_REG_ID);
	if (FIELD_GET(EDU_ID_MAGIC, ident) != EDU_ID_MAGIC_VAL)
		return -ENODEV;

	dev_info(&pdev->dev, "edu v%lu.%lu\n",
		 FIELD_GET(EDU_ID_MAJOR, ident), FIELD_GET(EDU_ID_MINOR, ident));

	return 0;
}

static struct pci_driver edu_npu_driver = {
	.name = KBUILD_MODNAME,
	.id_table = edu_npu_ids,
	.probe = edu_npu_probe,
};
module_pci_driver(edu_npu_driver);

MODULE_DESCRIPTION("Driver for the QEMU edu PCI device");
MODULE_LICENSE("GPL");
