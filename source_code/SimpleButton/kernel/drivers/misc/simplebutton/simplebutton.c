// SPDX-License-Identifier: GPL-2.0
/*
 * simplebutton.c — Linux misc device driver for simulated hardware button.
 *
 * Exposes sysfs nodes under /sys/class/misc/simplebutton/ and supports
 * ioctl + poll for userspace interaction.
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/device.h>

#define SIMPLEBUTTON_DEVICE_NAME "simplebutton"
#define SIMPLEBUTTON_MAGIC       'S'

/* ioctl commands */
#define SIMPLEBUTTON_IOCTL_READ    _IOR(SIMPLEBUTTON_MAGIC, 1, int)
#define SIMPLEBUTTON_IOCTL_TRIGGER _IO(SIMPLEBUTTON_MAGIC, 2)

static int button_value;
static DEFINE_SPINLOCK(button_lock);
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

/* ------------------------------------------------------------------ */
/* Sysfs: /sys/class/misc/simplebutton/value                          */
/* ------------------------------------------------------------------ */

static ssize_t simplebutton_value_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	unsigned long flags;
	int val;

	spin_lock_irqsave(&button_lock, flags);
	val = button_value;
	spin_unlock_irqrestore(&button_lock, flags);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static DEVICE_ATTR(value, 0444, simplebutton_value_show, NULL);

/* ------------------------------------------------------------------ */
/* Sysfs: /sys/class/misc/simplebutton/trigger                        */
/* ------------------------------------------------------------------ */

static ssize_t simplebutton_trigger_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	unsigned long flags;
	int trigger_val;
	int ret;

	ret = kstrtoint(buf, 10, &trigger_val);
	if (ret < 0)
		return ret;

	if (trigger_val == 1) {
		spin_lock_irqsave(&button_lock, flags);
		button_value = 1;
		spin_unlock_irqrestore(&button_lock, flags);

		pr_info("simplebutton: clicked\n");

		/* Reset value back to 0 after trigger, as per spec */
		spin_lock_irqsave(&button_lock, flags);
		button_value = 0;
		spin_unlock_irqrestore(&button_lock, flags);

		wake_up_interruptible(&button_waitq);
	}

	return count;
}

static DEVICE_ATTR(trigger, 0220, NULL, simplebutton_trigger_store);

/* ------------------------------------------------------------------ */
/* ioctl                                                              */
/* ------------------------------------------------------------------ */

static long simplebutton_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	unsigned long flags;
	int val;
	int __user *user_ptr = (int __user *)arg;

	switch (cmd) {
	case SIMPLEBUTTON_IOCTL_READ:
		spin_lock_irqsave(&button_lock, flags);
		val = button_value;
		spin_unlock_irqrestore(&button_lock, flags);

		if (copy_to_user(user_ptr, &val, sizeof(val)))
			return -EFAULT;
		return 0;

	case SIMPLEBUTTON_IOCTL_TRIGGER:
		spin_lock_irqsave(&button_lock, flags);
		button_value = 1;
		spin_unlock_irqrestore(&button_lock, flags);

		pr_info("simplebutton: clicked\n");

		spin_lock_irqsave(&button_lock, flags);
		button_value = 0;
		spin_unlock_irqrestore(&button_lock, flags);

		wake_up_interruptible(&button_waitq);
		return 0;

	default:
		return -ENOTTY;
	}
}

/* ------------------------------------------------------------------ */
/* poll — wake userspace when button state changes                    */
/* ------------------------------------------------------------------ */

static __poll_t simplebutton_poll(struct file *file, poll_table *wait)
{
	__poll_t mask = 0;

	poll_wait(file, &button_waitq, wait);

	/* Signal readable whenever a state change occurred */
	mask |= EPOLLIN | EPOLLRDNORM;
	return mask;
}

/* ------------------------------------------------------------------ */
/* file_operations                                                    */
/* ------------------------------------------------------------------ */

static const struct file_operations simplebutton_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = simplebutton_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = simplebutton_ioctl,
#endif
	.poll           = simplebutton_poll,
};

/* ------------------------------------------------------------------ */
/* misc device registration                                           */
/* ------------------------------------------------------------------ */

static struct miscdevice simplebutton_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = SIMPLEBUTTON_DEVICE_NAME,
	.fops  = &simplebutton_fops,
};

static int __init simplebutton_init(void)
{
	int ret;

	button_value = 0;

	ret = misc_register(&simplebutton_miscdev);
	if (ret) {
		pr_err("simplebutton: failed to register misc device: %d\n", ret);
		return ret;
	}

	ret = device_create_file(simplebutton_miscdev.this_device,
				 &dev_attr_value);
	if (ret) {
		pr_err("simplebutton: failed to create value sysfs node: %d\n", ret);
		misc_deregister(&simplebutton_miscdev);
		return ret;
	}

	ret = device_create_file(simplebutton_miscdev.this_device,
				 &dev_attr_trigger);
	if (ret) {
		pr_err("simplebutton: failed to create trigger sysfs node: %d\n", ret);
		device_remove_file(simplebutton_miscdev.this_device,
				   &dev_attr_value);
		misc_deregister(&simplebutton_miscdev);
		return ret;
	}

	pr_info("simplebutton: driver loaded (misc minor %d)\n",
		simplebutton_miscdev.minor);
	return 0;
}

static void __exit simplebutton_exit(void)
{
	device_remove_file(simplebutton_miscdev.this_device, &dev_attr_trigger);
	device_remove_file(simplebutton_miscdev.this_device, &dev_attr_value);
	misc_deregister(&simplebutton_miscdev);
	pr_info("simplebutton: driver unloaded\n");
}

module_init(simplebutton_init);
module_exit(simplebutton_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simple Energy");
MODULE_DESCRIPTION("Simulated hardware button misc device driver");
