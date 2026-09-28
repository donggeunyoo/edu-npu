// SPDX-License-Identifier: GPL-2.0

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/cleanup.h>
#include <linux/container_of.h>
#include <linux/dev_printk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "include/uapi/edu_npu.h"

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
	struct kref ref;
	void __iomem *regs;
	struct mutex lock; /* regs와 디바이스에 보내는 작업을 보호한다 */
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

static void edu_npu_free(struct kref *ref)
{
	struct edu_npu *npu = container_of(ref, struct edu_npu, ref);

	mutex_destroy(&npu->lock);
	kfree(npu);
}

static void edu_npu_put(void *data)
{
	struct edu_npu *npu = data;

	kref_put(&npu->ref, edu_npu_free);
}

static void edu_npu_unregister(void *data)
{
	struct edu_npu *npu = data;

	misc_deregister(&npu->misc);
	scoped_guard(mutex, &npu->lock)
		npu->regs = NULL;
}

static int edu_npu_open(struct inode *inode, struct file *file)
{
	struct edu_npu *npu = container_of(file->private_data, struct edu_npu,
					   misc);

	kref_get(&npu->ref);
	file->private_data = npu;
	return 0;
}

static int edu_npu_release(struct inode *inode, struct file *file)
{
	edu_npu_put(file->private_data);
	return 0;
}

static long edu_npu_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct edu_npu *npu = file->private_data;
	struct edu_npu_fact __user *uarg = (void __user *)arg;
	struct edu_npu_fact req;
	int ret;

	if (cmd != EDU_NPU_IOC_FACT)
		return -ENOTTY;

	if (copy_from_user(&req, uarg, sizeof(req)))
		return -EFAULT;

	scoped_guard(mutex, &npu->lock) {
		if (!npu->regs)
			ret = -ENODEV;
		else
			ret = edu_npu_factorial(npu->regs, req.n,
						&req.result);
	}
	if (ret)
		return ret;

	if (copy_to_user(uarg, &req, sizeof(req)))
		return -EFAULT;

	return 0;
}

static const struct file_operations edu_npu_fops = {
	.owner = THIS_MODULE,
	.open = edu_npu_open,
	.release = edu_npu_release,
	.unlocked_ioctl = edu_npu_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static int edu_npu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_npu *npu;
	void __iomem *regs;
	u32 ident, fact;
	int ret;

	npu = kzalloc_obj(*npu);
	if (!npu)
		return -ENOMEM;
	kref_init(&npu->ref);
	mutex_init(&npu->lock);

	ret = devm_add_action_or_reset(&pdev->dev, edu_npu_put, npu);
	if (ret)
		return ret;

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

	ret = edu_npu_factorial(regs, 5, &fact);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "5! = %u\n", fact);

	npu->regs = regs;
	npu->misc.minor = MISC_DYNAMIC_MINOR;
	npu->misc.name = KBUILD_MODNAME;
	npu->misc.fops = &edu_npu_fops;
	npu->misc.parent = &pdev->dev;

	ret = misc_register(&npu->misc);
	if (ret)
		return ret;

	return devm_add_action_or_reset(&pdev->dev, edu_npu_unregister,
					npu);
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
