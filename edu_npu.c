// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/err.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_PCI_VENDOR_ID	0x1234
#define EDU_PCI_DEVICE_ID	0x11e8

static const struct pci_device_id edu_npu_ids[] = {
	{ PCI_DEVICE(EDU_PCI_VENDOR_ID, EDU_PCI_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, edu_npu_ids);

static int edu_npu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	void __iomem *regs;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	regs = pcim_iomap_region(pdev, 0, KBUILD_MODNAME);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

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
