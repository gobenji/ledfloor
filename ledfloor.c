/* Copyright 2010 Benjamin Poirier, benjamin.poirier@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "ledfloor.h"

#ifdef CONFIG_AVR32
#include <asm/io.h>
#include <asm/sysreg.h>
#include <linux/gpio.h>
#include <mach/at32ap700x.h>
#else
/* Out of arch/avr32/mach-at32ap/include/mach/at32ap700x.h */
#define GPIO_PIOA_BASE	(0)
#define GPIO_PIOB_BASE	(GPIO_PIOA_BASE + 32)
#define GPIO_PIOC_BASE	(GPIO_PIOB_BASE + 32)
#define GPIO_PIOD_BASE	(GPIO_PIOC_BASE + 32)
#define GPIO_PIOE_BASE	(GPIO_PIOD_BASE + 32)

#define GPIO_PIN_PA(N)	(GPIO_PIOA_BASE + (N))
#define GPIO_PIN_PB(N)	(GPIO_PIOB_BASE + (N))
#define GPIO_PIN_PC(N)	(GPIO_PIOC_BASE + (N))
#define GPIO_PIN_PD(N)	(GPIO_PIOD_BASE + (N))
#define GPIO_PIN_PE(N)	(GPIO_PIOE_BASE + (N))

#define gpio_direction_output(gpio, value) 0
#define __raw_writel(v, addr)
#define sysreg_read(reg) 0
#define COUNT 0
#define gpio_set_value(gpio, value)
#define __raw_readl(addr) 0
#endif

/* Out of arch/avr32/mach-at32ap/pio.h */
#define PIO_SODR 0x0030 // Set Output Data Register
#define PIO_CODR 0x0034 // Clear Output Data Register
#define PIO_ODSR 0x0038 // Output Data Status Register
#define PIO_OWER 0x00a0 // Output Write Enable Register
#define PIO_OWSR 0x00a8 // Output Write Status Register

#define GPIO_HW_BASE 0xffe02800

#define GPIO_BANK(N) (N >> 5)
#define GPIO_INDEX(N) (N % 32)

static struct platform_device *ledfloor_gpio_device;
static struct class *ledfloor_class;
static struct ledfloor_dev_t {
	struct ledfloor_config *config;

	uint8_t buffer[LFCOLS * 3 * LFROWS];

	dev_t devid;
	struct cdev cdev;
	wait_queue_head_t wq;
	atomic_t fnum;
} dev = {
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(dev.wq),
	.fnum = ATOMIC_INIT(0),
};
static struct ledfloor_config
{
	int blank;
	int latch;
	int clk;
	int data[LFROWS];
	bool rotate; // 180 degrees rotation at no extra cost
	uint32_t latch_ndelay;
	uint32_t clk_ndelay;
} ledfloor_config_data = {
	.blank = GPIO_PIN_PA(29),
	.latch = GPIO_PIN_PA(30),
	.clk = GPIO_PIN_PA(31),
	.data = {
		GPIO_PIN_PB(4),
		GPIO_PIN_PB(3),
		GPIO_PIN_PB(0),
		GPIO_PIN_PB(5),
		GPIO_PIN_PB(2),
		GPIO_PIN_PB(1),
		GPIO_PIN_PB(16),
		GPIO_PIN_PB(15),
		GPIO_PIN_PB(12),
		GPIO_PIN_PB(17),
		GPIO_PIN_PB(14),
		GPIO_PIN_PB(13),
		GPIO_PIN_PB(11),
		GPIO_PIN_PB(6),
		GPIO_PIN_PB(7),
		GPIO_PIN_PB(10),
		GPIO_PIN_PB(9),
		GPIO_PIN_PB(8),
		GPIO_PIN_PB(19),
		GPIO_PIN_PB(18),
		GPIO_PIN_PB(21),
		GPIO_PIN_PB(20),
		GPIO_PIN_PB(23),
		GPIO_PIN_PB(22),
	},
	.rotate = false,
	.latch_ndelay = 0,
	.clk_ndelay = 200,
};
/* Gamma correction table, gamma = 2.2, upconvert 8 to 12 bits and reverse the
 * bit order
 * Generated using gammatable.py
 */
static uint16_t gamma_c[256] = {
	0, 0, 0, 0, 0, 2048, 2048, 1024, 1024, 3072, 3072, 512, 2560, 1536, 3584,
	256, 2304, 3328, 768, 1792, 3840, 2176, 3200, 2688, 3712, 2432, 3456,
	2944, 64, 1088, 2624, 320, 3392, 1856, 2240, 704, 3776, 3520, 1984, 1056,
	1568, 2336, 2848, 1184, 1696, 1440, 4000, 3168, 352, 2912, 1248, 3808,
	992, 2064, 3600, 784, 1168, 400, 1936, 592, 1360, 208, 1744, 3024, 560,
	1328, 2224, 432, 112, 3696, 1904, 1776, 2032, 2568, 2824, 2696, 1928,
	1608, 1864, 3784, 40, 296, 2216, 3496, 616, 2920, 3816, 24, 1304, 664,
	1944, 344, 3288, 3032, 312, 3256, 3000, 376, 760, 4088, 1284, 1668, 2116,
	2884, 2500, 2596, 1188, 1956, 3428, 3812, 532, 2196, 1940, 3412, 2516,
	1588, 692, 1140, 244, 2036, 780, 1420, 2380, 3788, 1580, 2732, 620, 748,
	3100, 1180, 1116, 1244, 1084, 1212, 1148, 3324, 3074, 642, 2626, 1730,
	3618, 418, 1378, 3554, 2834, 3986, 2258, 3122, 2738, 370, 1522, 2826, 74,
	3274, 1578, 1450, 2922, 2074, 2714, 2394, 3034, 2234, 2682, 1530, 3846,
	582, 2502, 1830, 3174, 2534, 1814, 598, 1494, 182, 1654, 3062, 3214, 1358,
	2094, 430, 3950, 1566, 1950, 1758, 2878, 2686, 3070, 1665, 1857, 3617,
	4001, 481, 2193, 3409, 561, 1969, 3825, 2185, 3401, 2601, 105, 1513, 2713,
	3929, 1337, 1657, 2053, 901, 453, 677, 3941, 789, 341, 565, 2165, 3061,
	1421, 3789, 685, 1261, 3869, 2909, 3389, 2429, 3587, 2627, 547, 1123,
	2067, 83, 4051, 4019, 2035, 1931, 1995, 2987, 2027, 1947, 2011, 4027, 7,
	2119, 1063, 3175, 535, 1623, 311, 1399, 783, 1871, 175, 3311, 1695, 2527,
	959, 4095
};

static int clk_mask, latch_mask;
static void *clk_reg_set, *clk_reg_clear;
static void *latch_reg_set, *latch_reg_clear;
static size_t row_offsets[LFROWS];
static void *data_reg;

/* The next functions access GPIO registers directly to bypass many function
 * call levels and, more importantly, write many bits at once on one port.
 * This is sketchy because it bypasses the whole gpio framework, but it's way
 * faster. The addresses and register offsets were obtained by looking at the
 * PIO driver and confirmed with the AP7000 datasheet.
 */
static int __init gpio_init(const struct ledfloor_config *config)
{
	unsigned int i;
	int errno;
	unsigned int reverse_index[LFROWS];

	if ((errno = gpio_direction_output(config->blank, 0))) {
		printk(KERN_ERR "ledfloor gpio_init, failed to "
			"register blank line\n");
		return errno;
	}
	if ((errno = gpio_direction_output(config->latch, 0))) {
		printk(KERN_ERR "ledfloor gpio_init, failed to "
			"register latch line\n");
		return errno;
	}
	if ((errno = gpio_direction_output(config->clk, 0))) {
		printk(KERN_ERR "ledfloor gpio_init, failed to "
			"register clock line\n");
		return errno;
	}

	for (i = 0; i < ARRAY_SIZE(config->data); i++)
	{
		if ((errno = gpio_direction_output(config->data[i], 0))) {
			printk(KERN_ERR "ledfloor gpio_init, failed to "
				"register data line %d\n", i);
			return errno;
		}
	}

	clk_mask = 1 << GPIO_INDEX(config->clk);
	clk_reg_set = (void*) (GPIO_HW_BASE + GPIO_BANK(config->clk) * 0x400 +
		PIO_SODR);
	clk_reg_clear = (void*) (GPIO_HW_BASE + GPIO_BANK(config->clk) * 0x400
		+ PIO_CODR);

	latch_mask = 1 << GPIO_INDEX(config->latch);
	latch_reg_set = (void*) (GPIO_HW_BASE + GPIO_BANK(config->latch) *
		0x400 + PIO_SODR);
	latch_reg_clear = (void*) (GPIO_HW_BASE + GPIO_BANK(config->latch) *
		0x400 + PIO_CODR);

	/* reverse_index[i] = buffer row that contains the pixel output on
	 * data line i */
	for (i = 0; i < LFROWS; i++) {
		reverse_index[config->data[i] & ((1 << 5) - 1)] =
			config->rotate ? LFROWS - 1 - i : i;
	}
	/* row_offsets[i] = offset relative to a pixel on the first row to get
	 * the pixel on the row output on data line i */
	for (i = 0; i < LFROWS; i++) {
		row_offsets[i] = reverse_index[i] * LFCOLS * 3;
	}

	data_reg = (void*) (GPIO_HW_BASE + GPIO_BANK(config->data[0]) * 0x400
		+ PIO_ODSR);

	return 0;
}

static inline void output_col_component(uint8_t *buffer, const struct
	ledfloor_config *config, const unsigned int i)
{
	int j, k;
	/* Only the first 12 bits may be set */
	uint16_t component_values[LFROWS];

	for (j = 0; j < ARRAY_SIZE(component_values); j++) {
		component_values[j] = gamma_c[buffer[i + row_offsets[j]]];
	}

	for (k = 0; k < 12; k++) {
		uint32_t output_value = 0;

		__raw_writel(clk_mask, clk_reg_clear);

		for (j = ARRAY_SIZE(component_values) - 1; j >= 0; j--) {
			output_value <<= 1;
			output_value |= component_values[j] & 1;
			component_values[j] <<= 1;
		}
		__raw_writel(output_value, data_reg);

		ndelay(config->clk_ndelay);
		__raw_writel(clk_mask, clk_reg_set);
		ndelay(config->clk_ndelay);
	}
}

static void write_frame(uint8_t *buffer, const struct ledfloor_config *config)
{
	int i;
	uint32_t write_mask;
	//unsigned long start;

	//start = sysreg_read(COUNT);

	// LED "B" is active low
	gpio_set_value(GPIO_PIN_PE(19), 0);

	write_mask = __raw_readl((void*) (0xffe02800 + ((config->data[0] >> 5)
				* 0x400) + PIO_OWSR));
	__raw_writel((1 << LFROWS) - 1, (void*) (0xffe02800 + ((config->data[0] >>
					5) * 0x400) + PIO_OWER));

	__raw_writel(latch_mask, latch_reg_clear);
	if (config->rotate) {
		for (i = 0; i < LFCOLS * 3; i++) {
			output_col_component(buffer, config, i);
		}
	}
	else {
		for (i = LFCOLS * 3 - 1; i >= 0; i--) {
			output_col_component(buffer, config, i);
		}
	}
	ndelay(config->latch_ndelay);
	__raw_writel(latch_mask, latch_reg_set);
	ndelay(config->latch_ndelay);

	__raw_writel(write_mask, (void*) (0xffe02800 + ((GPIO_PIOB_BASE >> 5) *
				0x400) + PIO_OWSR));

	gpio_set_value(GPIO_PIN_PE(19), 1);

#ifndef CONFIG_AVR32
	printk(KERN_INFO "ledfloor write_frame\n");
#endif

	//printk(KERN_INFO "ledfloor write_frame in %lu cycles\n", sysreg_read(COUNT) - start);
}

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
	int i = atomic_read(&dev->fnum);

	if (*f_pos >= LFCOLS * 3 * LFROWS) {
		return 0;
	}
	if (*f_pos + count > LFCOLS * 3 * LFROWS) {
		count = LFCOLS * 3 * LFROWS - *f_pos;
	}

	if (!(filp->f_flags & O_NONBLOCK) && *f_pos == 0 &&
		wait_event_interruptible(dev->wq, atomic_read(&dev->fnum) != i)) {
		return -ERESTARTSYS;
	}

	if (copy_to_user(buf, &dev->buffer[*f_pos], count)) {
		return -EFAULT;
	}

	*f_pos += count;
	BUG_ON(*f_pos > LFCOLS * 3 * LFROWS);
	if (*f_pos == LFCOLS * 3 * LFROWS) {
		*f_pos = 0;
	}

	return count;
}

static ssize_t ledfloor_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct ledfloor_dev_t *dev = filp->private_data;
	size_t left_to_write = count;

	if (*f_pos >= LFCOLS * 3 * LFROWS) {
		return 0;
	}

	while (left_to_write)
	{
		size_t copy_count = left_to_write;

		if (*f_pos + copy_count > LFCOLS * 3 * LFROWS) {
			copy_count = LFCOLS * 3 * LFROWS - *f_pos;
		}

		if (copy_from_user(&dev->buffer[*f_pos], buf, copy_count)) {
			return -EFAULT;
		}
		left_to_write -= copy_count;
		*f_pos += copy_count;
		BUG_ON(*f_pos > LFCOLS * 3 * LFROWS);
		if (*f_pos == LFCOLS * 3 * LFROWS) {
			*f_pos = 0;
			write_frame(dev->buffer, dev->config);
			atomic_inc(&dev->fnum);
			wake_up_interruptible(&dev->wq);
		}
	}

	return count;
}

static int ledfloor_ioctl(struct inode *inode, struct file *filp, unsigned
	int cmd, unsigned long arg)
{
	struct ledfloor_dev_t *dev = filp->private_data;
	int err = 0;
	int retval = 0;

	if (_IOC_TYPE(cmd) != LF_IOC_MAGIC) {
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) >= LF_IOC_NB) {
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE, (void __user *) arg,
			_IOC_SIZE(cmd));
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok(VERIFY_READ, (void __user *) arg,
			_IOC_SIZE(cmd));
	}
	if (err) {
		return -EFAULT;
	}

	switch (cmd) {
		case LF_IOCSLATCHNDELAY:
			retval = __get_user(dev->config->latch_ndelay,
				(uint32_t __user *) arg);
#ifndef CONFIG_AVR32
			printk(KERN_INFO "ledfloor latch_ndelay = %u\n",
				dev->config->latch_ndelay);
#endif
			break;

		case LF_IOCSCLKNDELAY:
			retval = __get_user(dev->config->clk_ndelay,
				(uint32_t __user *) arg);
#ifndef CONFIG_AVR32
			printk(KERN_INFO "ledfloor clk_ndelay = %u\n",
				dev->config->clk_ndelay);
#endif
			break;

		default:
			/* Command number has already been checked */
			BUG();
	}

	return retval;
}

struct file_operations ledfloor_fops = {
	.owner = THIS_MODULE,
	.read = ledfloor_read,
	.write = ledfloor_write,
	.ioctl = ledfloor_ioctl,
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

	memset(dev.buffer, 0, LFCOLS * 3 * LFROWS);

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

	device_create(ledfloor_class, NULL, dev.devid, NULL, "ledfloor%d",
		MINOR(dev.devid));

	return 0;
}


static int __exit platform_ledfloor_remove(struct platform_device *pdev)
{
	dev_notice(&pdev->dev, "remove() called\n");

	device_destroy(ledfloor_class, dev.devid);
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


static struct platform_driver ledfloor_driver = {
	.probe		= &platform_ledfloor_probe,
	.remove		= __exit_p(&platform_ledfloor_remove),
	.suspend	= &platform_ledfloor_suspend,
	.resume		= &platform_ledfloor_resume,
	.driver	= {
		.name	= "ledfloor",
	},
};

static int __init ledfloor_init(void)
{
	int ret;

	printk(KERN_INFO "ledfloor init\n");
	ledfloor_class = class_create(THIS_MODULE, "ledfloor");

	ret = -ENOMEM;
	ledfloor_gpio_device = platform_device_alloc("ledfloor", 0);
	if (!ledfloor_gpio_device) {
		goto fail;
	}

	/* Note that the data is copied into a new dynamically allocated
	 * structure.
	 */
	ret = platform_device_add_data(ledfloor_gpio_device,
		&ledfloor_config_data, sizeof(ledfloor_config_data));
	if (ret) {
		goto fail;
	}

	printk(KERN_INFO "ledfloor registering device \"%s.%d\"...\n",
		ledfloor_gpio_device->name, ledfloor_gpio_device->id);
	ret = platform_device_add(ledfloor_gpio_device);
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
	platform_device_put(ledfloor_gpio_device);

	class_destroy(ledfloor_class);

	return ret;
}
module_init(ledfloor_init);


static void __exit ledfloor_exit(void)
{
	printk(KERN_INFO "ledfloor exit\n");

	platform_driver_unregister(&ledfloor_driver);
	class_destroy(ledfloor_class);

	printk(KERN_INFO "ledfloor removing device \"%s.%d\"...\n",
		ledfloor_gpio_device->name, ledfloor_gpio_device->id);

	platform_device_del(ledfloor_gpio_device);
}
module_exit(ledfloor_exit);


MODULE_DESCRIPTION("LED dance floor framebuffer through avr32 GPIO pins");
MODULE_AUTHOR("Benjamin Poirier <benjamin.poirier@gmail.com>");
MODULE_LICENSE("GPL");
