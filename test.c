#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <asm/uaccess.h> /* copy_from_user, copy_to_user */
#include <linux/kfifo.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Me");
MODULE_LICENSE("GPL");

#define DUMMY_MINOR 0
#define DUMMY_NUM_MINORS 1
#define DUMMY_MODULE_NAME "dummy"

#define FIFO_SIZE 100

int openCount = 0;
int closeCount = 0;

struct dummy_device_data {
	struct cdev cdev;
	dev_t dev;
};

static struct kfifo test;
static struct dummy_device_data dummy_device_data[DUMMY_NUM_MINORS];

ssize_t dummy_read (struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copied;

	ret = kfifo_to_user(&test, buf, 1, &copied);

	return ret ? ret : copied;
}

ssize_t dummy_write (struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copied;

	ret = kfifo_from_user(&test, buf, count, &copied);

	return ret ? ret : copied;
}


int dummy_open (struct inode *pinode, struct file *pfile)
{
	openCount++;
	printk(KERN_ALERT "[OPENING] It has been opened %d times\n", openCount);
	return 0;
}


int dummy_close (struct inode *pinode, struct file *pfile)
{
	closeCount++;
	printk(KERN_ALERT "[CLOSING] It has been closed %d times\n", closeCount);
	return 0;
}

struct file_operations dummy_fops = {
	.owner   = THIS_MODULE,
	.read    = dummy_read,
	.write   = dummy_write,
	.open    = dummy_open,
	.release = dummy_close
};

static int dummy_chrdev_init(void)
{
	int error;
	dev_t first_dev;
	unsigned int major, first_minor;
	int i;

	error = alloc_chrdev_region(&first_dev, DUMMY_MINOR, DUMMY_NUM_MINORS,
				    DUMMY_MODULE_NAME);
	if (error) {
		pr_err("%s: alloc_chrdev_region failed: %d\n",
		       __FUNCTION__, error);
		return error;
	}

	major = MAJOR(first_dev);
	first_minor = MINOR(first_dev);
	BUG_ON(first_minor + DUMMY_NUM_MINORS > MINORMASK);

	for (i = 0; i < DUMMY_NUM_MINORS; i++) {
		dummy_device_data[i].dev = MKDEV(major, first_minor + i);
	}

	return 0;
}

static int dummy_cdev_init(struct dummy_device_data *device_data)
{
	int error;
	struct cdev *cdev = &device_data->cdev;

	cdev_init(cdev, &dummy_fops);
	error = cdev_add(cdev, device_data->dev, 1);
	if (error) {
		pr_err("%s: cdev_add failed: %d\n",
		       __FUNCTION__, error);
	}

	printk("%s: created character device %x:%x (ma:mi).\n", DUMMY_MODULE_NAME,
	       MAJOR(device_data->dev), MINOR(device_data->dev));

	return error;
}

static int dummy_init(void)
{
	int error = 0;
	int i;

	printk("%s: Hi\n", __FUNCTION__);

	error = dummy_chrdev_init();
	if (error) {
		pr_err("%s: dummy_dev_init failed: %d\n",
		       __FUNCTION__, error);
		return error;
	}

	for (i = 0; i < DUMMY_NUM_MINORS && !error; i++) {
		error = dummy_cdev_init(&dummy_device_data[i]);
	}

	if (error) {
		pr_err("%s: dummy_cdev_init failed: %d\n",
			 __FUNCTION__, error);
	}


	/* Initializing fifo */
	int ret;

	ret = kfifo_alloc(&test, FIFO_SIZE, GFP_KERNEL);
	if (ret) {
		printk(KERN_ERR "error kfifo_alloc\n");
		return ret;
	}

        return 0;
}

static void dummy_exit(void)
{
	printk(KERN_ALERT "[EXITING]\n");
	kfifo_free(&test);
}


module_init(dummy_init);
module_exit(dummy_exit);
