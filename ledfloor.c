#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "ledfloor.h"

#ifdef CONFIG_AVR32
#include <linux/gpio.h>
#include <mach/at32ap700x.h>
#endif

#define ROWS 24
#define COLS 48


struct ledfloor_dev_t {
	const struct ledfloor_config *config;

	uint8_t buffer[COLS * 3 * ROWS];

	dev_t devid;
	struct cdev cdev;
} dev;


#ifdef CONFIG_AVR32
static int __init gpio_init(const struct ledfloor_config *config)
{
	unsigned int i;
	int errno;

	if ((errno = gpio_direction_output(config->ce, 1))) {
		return errno;
	}

	for (i = 0; i < ARRAY_SIZE(config->a); i++)
	{
		if ((errno = gpio_direction_output(config->a[i], 0))) {
			return errno;
		}
	}

	for (i = 0; i < ARRAY_SIZE(config->data); i++)
	{
		if ((errno = gpio_direction_output(config->data[i], 0))) {
			return errno;
		}
	}

	return 0;
}


static void write_frame(uint8_t *buffer, const struct ledfloor_config *config)
{
	unsigned int i, bit;

	for (i = 0; i < COLS * 3 * ROWS; i++) {
		for (bit = 0; bit < ARRAY_SIZE(config->a); bit++)
		{
			gpio_set_value(config->a[bit], i && 1 << bit);
		}
		gpio_set_value(config->ce, 0);
		for (bit = 0; bit < ARRAY_SIZE(config->data); bit++)
		{
			gpio_set_value(config->data[bit],
				buffer[i] && 1 << bit);
		}
		gpio_set_value(config->ce, 1);
	}
}
#else
static int __init gpio_init(const struct ledfloor_config *config)
{
	return 0;
}
static void write_frame(uint8_t *buffer, const struct ledfloor_config *config)
{
	printk(KERN_INFO "ledfloor write_frame\n");
}
#endif


static int ledfloor_open(struct inode *inode, struct file *filp)
{
	struct ledfloor_dev_t *dev = container_of(inode->i_cdev, struct
		ledfloor_dev_t, cdev);

	filp->private_data = dev;

	return 0;
}


static int ledfloor_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static ssize_t ledfloor_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct ledfloor_dev_t *dev = filp->private_data;

	if (*f_pos >= COLS * 3 * ROWS) {
		return 0;
	}
	if (*f_pos + count > COLS * 3 * ROWS) {
		count = COLS * 3 * ROWS - *f_pos;
	}

	if (copy_to_user(buf, &dev->buffer[*f_pos], count)) {
		return -EFAULT;
	}

	*f_pos += count;
	BUG_ON(*f_pos > COLS * 3 * ROWS);
	if (*f_pos == COLS * 3 * ROWS) {
		*f_pos = 0;
	}

	return count;
}


static ssize_t ledfloor_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct ledfloor_dev_t *dev = filp->private_data;
	size_t left_to_write = count;

	if (*f_pos >= COLS * 3 * ROWS) {
		return 0;
	}

	while (left_to_write)
	{
		size_t copy_count = left_to_write;

		if (*f_pos + copy_count > COLS * 3 * ROWS) {
			copy_count = COLS * 3 * ROWS - *f_pos;
		}

		if (copy_from_user(&dev->buffer[*f_pos], buf, copy_count)) {
			return -EFAULT;
		}
		left_to_write -= copy_count;
		*f_pos += copy_count;
		BUG_ON(*f_pos > COLS * 3 * ROWS);
		if (*f_pos == COLS * 3 * ROWS) {
			*f_pos = 0;
			write_frame(dev->buffer, dev->config);
		}
	}

	return count;
}

struct file_operations ledfloor_fops = {
	.owner = THIS_MODULE,
	//.llseek = ledfloor_llseek,
	.read = ledfloor_read,
	.write = ledfloor_write,
	//.ioctl = ledfloor_ioctl,
	.open = ledfloor_open,
	.release = ledfloor_release,
};


static int __init platform_ledfloor_probe(struct platform_device *pdev)
{
	int ret;
	dev.config = pdev->dev.platform_data;
	
	dev_notice(&pdev->dev, "probe() called\n");
	
	ret= gpio_init(dev.config);
	if (ret < 0) {
		dev_warn(&pdev->dev, "gpio_init() failed\n");
		return ret;
	}

	memset(dev.buffer, 0, COLS * 3 * ROWS);

	ret = alloc_chrdev_region(&dev.devid, 0, 1, "ledfloor");
	if (ret < 0) {
		dev_warn(&pdev->dev, "ledfloor: can't get major number\n");
		return ret;
	}

	cdev_init(&dev.cdev, &ledfloor_fops);
	dev.cdev.owner = THIS_MODULE;
	dev.cdev.ops = &ledfloor_fops;

	ret = cdev_add(&dev.cdev, dev.devid, 1);
	if (ret < 0) {
		printk(KERN_WARNING "ledfloor: can't add device\n");
		return ret;
	}

	return 0;
}


static int __exit platform_ledfloor_remove(struct platform_device *pdev)
{
	dev_notice(&pdev->dev, "remove() called\n");

	cdev_del(&dev.cdev);
	unregister_chrdev_region(dev.devid, 1);

	return 0;
}


#ifdef CONFIG_PM
static int
platform_ledfloor_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* Add code to suspend the device here */

	dev_notice(&pdev->dev, "suspend() called\n");

	return 0;
}

static int platform_ledfloor_resume(struct platform_device *pdev)
{
	/* Add code to resume the device here */

	dev_notice(&pdev->dev, "resume() called\n");

	return 0;
}
#else
/* No need to do suspend/resume if power management is disabled */
# define platform_ledfloor_suspend NULL
# define platform_ledfloor_resume NULL
#endif


#ifdef CONFIG_AVR32
static struct platform_driver ledfloor_driver = {
	.remove		= __exit_p(&platform_ledfloor_remove),
	.suspend	= &platform_ledfloor_suspend,
	.resume		= &platform_ledfloor_resume,
	.driver	= {
		.name	= "ledfloor",
	},
};

static int __init ledfloor_init(void)
{
	printk(KERN_INFO "ledfloor init\n");
	return platform_driver_probe(&ledfloor_driver,
		&platform_ledfloor_probe);
}
#else
static struct platform_driver ledfloor_driver = {
	.probe		= &platform_ledfloor_probe,
	.remove		= __exit_p(&platform_ledfloor_remove),
	.suspend	= &platform_ledfloor_suspend,
	.resume		= &platform_ledfloor_resume,
	.driver	= {
		.name	= "ledfloor",
	},
};

static struct platform_device *device;

static int __init ledfloor_init(void)
{
	int ret;
	struct ledfloor_config pin_config = {
		.ce = 0,
		.a = {
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
		},
		.data = {
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
		},
	};

	printk(KERN_INFO "ledfloor init\n");

	ret = -ENOMEM;
	device = platform_device_alloc("ledfloor", 0);
	if (!device) {
		goto fail;
	}

	/*
	 * The data is copied into a new dynamically allocated
	 * structure, so it's ok to pass variables defined on
	 * the stack here.
	 */
	ret = platform_device_add_data(device, &pin_config,
		sizeof(pin_config));
	if (ret) {
		goto fail;
	}

	printk(KERN_INFO "ledfloor registering device \"%s.%d\"...\n",
		device->name, device->id);
	ret = platform_device_add(device);
	if (ret) {
		goto fail;
	}

	return platform_driver_register(&ledfloor_driver);
fail:
	/*
	 * The device was never registered, so we may free it
	 * directly. Any dynamically allocated resources and
	 * platform data will be freed automatically.
	 */
	platform_device_put(device);

	return ret;
}
#endif
module_init(ledfloor_init);


static void __exit ledfloor_exit(void)
{
	printk(KERN_INFO "ledfloor exit\n");

	platform_driver_unregister(&ledfloor_driver);

#ifndef CONFIG_AVR32
	printk(KERN_INFO "ledfloor removing device \"%s.%d\"...\n",
		device->name, device->id);

	platform_device_del(device);
#endif
}
module_exit(ledfloor_exit);


MODULE_DESCRIPTION("LED dance floor framebuffer through avr32 GPIO pins");
MODULE_AUTHOR("Benjamin Poirier <benjamin.poirier@gmail.com>");
MODULE_LICENSE("GPL");
