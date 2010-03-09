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
	unsigned int latch_ndelay;
	unsigned int clk_ndelay;
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
/* Gamma correction table, gamma = 2.2, upconvert 8 to 12 bits
 * Generated using:
 * python -c 'print ", ".join([str(int(round((float(i) / 255)**(2.2) * 4095)))
 * for i in range(256)])'
 */
static uint16_t gamma_c[256] = {
	// linéaire
	/*
	0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240,
	256, 272, 288, 304, 320, 336, 352, 368, 384, 400, 416, 432, 448, 464, 480,
	496, 512, 528, 544, 560, 576, 592, 608, 624, 640, 656, 672, 688, 704, 720,
	736, 752, 768, 784, 800, 816, 832, 848, 864, 880, 896, 912, 928, 944, 960,
	976, 992, 1008, 1024, 1040, 1056, 1072, 1088, 1104, 1120, 1136, 1152,
	1168, 1184, 1200, 1216, 1232, 1248, 1264, 1280, 1296, 1312, 1328, 1344,
	1360, 1376, 1392, 1408, 1424, 1440, 1456, 1472, 1488, 1504, 1520, 1536,
	1552, 1568, 1584, 1600, 1616, 1632, 1648, 1664, 1680, 1696, 1712, 1728,
	1744, 1760, 1776, 1792, 1808, 1824, 1840, 1856, 1872, 1888, 1904, 1920,
	1936, 1952, 1968, 1984, 2000, 2016, 2032, 2048, 2064, 2080, 2096, 2112,
	2128, 2144, 2160, 2176, 2192, 2208, 2224, 2240, 2256, 2272, 2288, 2304,
	2320, 2336, 2352, 2368, 2384, 2400, 2416, 2432, 2448, 2464, 2480, 2496,
	2512, 2528, 2544, 2560, 2576, 2592, 2608, 2624, 2640, 2656, 2672, 2688,
	2704, 2720, 2736, 2752, 2768, 2784, 2800, 2816, 2832, 2848, 2864, 2880,
	2896, 2912, 2928, 2944, 2960, 2976, 2992, 3008, 3024, 3040, 3056, 3072,
	3088, 3104, 3120, 3136, 3152, 3168, 3184, 3200, 3216, 3232, 3248, 3264,
	3280, 3296, 3312, 3328, 3344, 3360, 3376, 3392, 3408, 3424, 3440, 3456,
	3472, 3488, 3504, 3520, 3536, 3552, 3568, 3584, 3600, 3616, 3632, 3648,
	3664, 3680, 3696, 3712, 3728, 3744, 3760, 3776, 3792, 3808, 3824, 3840,
	3856, 3872, 3888, 3904, 3920, 3936, 3952, 3968, 3984, 4000, 4016, 4032,
	4048, 4064, 4080
	*/
	// gamma = 2.2
	0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 11, 12, 14, 15, 17,
	19, 21, 23, 25, 27, 29, 32, 34, 37, 40, 43, 46, 49, 52, 55, 59, 62,
	66, 70, 73, 77, 82, 86, 90, 95, 99, 104, 109, 114, 119, 124, 129, 135,
	140, 146, 152, 158, 164, 170, 176, 182, 189, 196, 202, 209, 216, 224,
	231, 238, 246, 254, 261, 269, 277, 286, 294, 302, 311, 320, 328, 337,
	347, 356, 365, 375, 384, 394, 404, 414, 424, 435, 445, 456, 467, 477,
	488, 500, 511, 522, 534, 545, 557, 569, 581, 594, 606, 619, 631, 644,
	657, 670, 683, 697, 710, 724, 738, 752, 766, 780, 794, 809, 823, 838,
	853, 868, 884, 899, 914, 930, 946, 962, 978, 994, 1011, 1027, 1044,
	1061, 1078, 1095, 1112, 1130, 1147, 1165, 1183, 1201, 1219, 1237,
	1256, 1274, 1293, 1312, 1331, 1350, 1370, 1389, 1409, 1429, 1449,
	1469, 1489, 1509, 1530, 1551, 1572, 1593, 1614, 1635, 1657, 1678,
	1700, 1722, 1744, 1766, 1789, 1811, 1834, 1857, 1880, 1903, 1926,
	1950, 1974, 1997, 2021, 2045, 2070, 2094, 2119, 2143, 2168, 2193,
	2219, 2244, 2270, 2295, 2321, 2347, 2373, 2400, 2426, 2453, 2479,
	2506, 2534, 2561, 2588, 2616, 2644, 2671, 2700, 2728, 2756, 2785,
	2813, 2842, 2871, 2900, 2930, 2959, 2989, 3019, 3049, 3079, 3109,
	3140, 3170, 3201, 3232, 3263, 3295, 3326, 3358, 3390, 3421, 3454,
	3486, 3518, 3551, 3584, 3617, 3650, 3683, 3716, 3750, 3784, 3818,
	3852, 3886, 3920, 3955, 3990, 4025, 4060, 4095
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
			output_value |= (component_values[j] & (1 << 11)) >> 11;
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
				(unsigned int __user *) arg);
			break;

		case LF_IOCSCLKNDELAY:
			retval = __get_user(dev->config->latch_ndelay,
				(unsigned int __user *) arg);
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
