#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>

#ifdef CONFIG_AVR32
#include <linux/gpio.h>
#include <mach/at32ap700x.h>
#endif

#define ROWS 24
#define COLS 48


static int ledfloor_open(struct inode *inode, struct file *filp);
static int ledfloor_release(struct inode *inode, struct file *filp);
static ssize_t ledfloor_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos);
static ssize_t ledfloor_write(struct file *filp, const char __user *buf, size_t
	count, loff_t *f_pos);


dev_t devid;
struct ledfloor_dev_t {
	uint8_t buffer[COLS][3][ROWS];
	struct cdev cdev;
} ledfloor_dev;
struct file_operations ledfloor_fops = {
	.owner = THIS_MODULE,
	//.llseek = ledfloor_llseek,
	.read = ledfloor_read,
	.write = ledfloor_write,
	//.ioctl = ledfloor_ioctl,
	.open = ledfloor_open,
	.release = ledfloor_release,
};
#ifdef CONFIG_AVR32
struct
{
	int ce;
	int a[11];
	int data[8];
} pin_config = {
	.ce = GPIO_PIN_PA(0),
	.a = {
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
	},
	.data = {
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
	},
};


static int gpio_init(void)
{
	unsigned int i;
	int errno;

	if ((errno = gpio_direction_output(pin_config.ce, 1))) {
		return errno;
	}

	for (i = 0; i < ARRAY_SIZE(pin_config.a); i++)
	{
		if ((errno = gpio_direction_output(pin_config.a[i], 0))) {
			return errno;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pin_config.data); i++)
	{
		if ((errno = gpio_direction_output(pin_config.data[i], 0))) {
			return errno;
		}
	}

	return 0;
}


static void write_frame(uint8_t ***buffer)
{
	unsigned int col, comp, row, bit;

	for (col = 0; col < COLS; col++) {
		for (comp = 0; comp < 3; comp++) {
			for (row = 0; row < ROWS; row++) {
				gpio_set_value(pin_config.a[11], row);
				gpio_set_value(pin_config.ce, 0);
				for (bit = 0; bit < 8; bit++)
				{
					gpio_set_value(pin_config.data[bit],
						buffer[col][row][comp] &&
						1 << bit);
				}
				gpio_set_value(pin_config.ce, 1);
			}
		}
	}
}
#else
static inline int gpio_init(void)
{
	return 0;
}
static inline void write_frame(uint8_t ***buffer)
{}
#endif


static int __init ledfloor_init(void)
{
	int errno;
	
	printk(KERN_INFO "ledfloor init\n");
	
	memset(ledfloor_dev.buffer, 0, ROWS * COLS * 3);
	gpio_init();

	errno = alloc_chrdev_region(&devid, 0, 1, "ledfloor");

	if (errno < 0) {
		printk(KERN_WARNING "ledfloor: can't get major number\n");
		return errno;
	}

	cdev_init(&ledfloor_dev.cdev, &ledfloor_fops);
	ledfloor_dev.cdev.owner = THIS_MODULE;
	ledfloor_dev.cdev.ops = &ledfloor_fops;

	errno = cdev_add(&ledfloor_dev.cdev, devid, 1);
	if (errno < 0) {
		printk(KERN_WARNING "ledfloor: can't add device\n");
		return errno;
	}

	return 0;
}
module_init(ledfloor_init);


static void __exit ledfloor_exit(void)
{
	printk(KERN_INFO "ledfloor exit\n");

	cdev_del(&ledfloor_dev.cdev);
	unregister_chrdev_region(devid, 1);
}
module_exit(ledfloor_exit);


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

	if (*f_pos >= ROWS * COLS * 3) {
		return 0;
	}
	if (*f_pos + count > ROWS * COLS * 3) {
		count = ROWS * COLS * 3 - *f_pos;
	}

	if (copy_to_user(buf, &dev->buffer[*f_pos], count)) {
		return -EFAULT;
	}

	*f_pos += count;
	if (*f_pos >= ROWS * COLS * 3) {
		*f_pos -= ROWS * COLS * 3;
	}

	return count;
}


static ssize_t ledfloor_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct ledfloor_dev_t *dev = filp->private_data;
	size_t left = count;

	if (*f_pos >= ROWS * COLS * 3) {
		return 0;
	}

	while (left)
	{
		size_t copy_count;

		if (*f_pos + left > ROWS * COLS * 3) {
			copy_count = ROWS * COLS * 3 - *f_pos;
		}
		else {
			copy_count = left;
		}

		if (copy_from_user(&dev->buffer[*f_pos], buf, copy_count)) {
			return -EFAULT;
		}
		left -= copy_count;
		*f_pos += copy_count;
		if (*f_pos >= ROWS * COLS * 3) {
			*f_pos -= ROWS * COLS * 3;
			write_frame(dev->buffer);
		}
	}

	return count;
}


MODULE_DESCRIPTION("LED dance floor framebuffer through avr32 GPIO pins");
MODULE_AUTHOR("Benjamin Poirier <benjamin.poirier@gmail.com>");
MODULE_LICENSE("GPL");
