#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Me");
MODULE_LICENSE("GPL");

#define DUMMY_MINOR 0
#define DUMMY_NUM_MINORS 1
#define DUMMY_MODULE_NAME "dummy"

#define MSG_QUEUE_LEN 50
#define MSG_BUFFER_LEN 3330

struct node
{
	char *buffer;
	size_t len;
};

struct dummy_device_data {
	struct cdev cdev;
	dev_t dev;
	struct node msg_queue[MSG_QUEUE_LEN];
	size_t msg_queue_len;
};

//static DEFINE_SPINLOCK(msg_queue_rw_lock);
//static DECLARE_MUTEX(msg_queue_rw_mutex);

static int flag;
static struct dummy_device_data dummy_device_data[DUMMY_NUM_MINORS];

static int dummy_open (struct inode * inode, struct file * file)
{
	printk ("%s: opened.\n", DUMMY_MODULE_NAME);
	flag = 1;
	return 0;
}

static int dummy_release (struct inode * inode, struct file * file)
{
	printk ("%s: closed.\n", DUMMY_MODULE_NAME);
	return 0;
}

static ssize_t dummy_read (struct file * file, char __user * user_buffer,
					size_t size, loff_t * offset)
{
	struct dummy_device_data *d = &dummy_device_data[0];
	struct node *n = &(d->msg_queue[0]);
	size_t len = n->len;

//	spin_lock(&msg_queue_rw_lock);
//	if (down_interruptible (&msg_queue_rw_mutex))
//		return -EINTR;

	if ((size <= 0) || (d->msg_queue_len <= 0) || (!flag))
	{
//		spin_unlock(&msg_queue_rw_lock);
//		up (&msg_queue_rw_mutex);
    	return 0;
	}

	if (copy_to_user(user_buffer, n->buffer, len))
	{
//		spin_unlock(&msg_queue_rw_lock);
//		up (&msg_queue_rw_mutex);
		return -EFAULT;
	}

	vfree (n->buffer);
	memmove (n, n + 1, (d->msg_queue_len - 1) * sizeof (struct node));

	d->msg_queue_len--;
	flag--;

//	spin_unlock(&msg_queue_rw_lock);
//	up (&msg_queue_rw_mutex);
    
	return len;
}

static ssize_t dummy_write (struct file * file,
							const char __user * user_buffer,
					 		size_t size, loff_t * offset)
{
	struct dummy_device_data *d = &dummy_device_data[0];
	struct node *n = &(d->msg_queue[d->msg_queue_len]);
	
//	spin_lock(&msg_queue_rw_lock);
//	if (down_interruptible (&msg_queue_rw_mutex))
//		return -EINTR;
	
	if (size <= 0 || d->msg_queue_len >= MSG_QUEUE_LEN)
	{
//		spin_unlock(&msg_queue_rw_lock);
//		up (&msg_queue_rw_mutex);
		return 0;
	}

	n->buffer = NULL;
	n->buffer = (char *) vmalloc (size * sizeof(char));
	if (!n->buffer)
	{
//		spin_unlock(&msg_queue_rw_lock);
//		up (&msg_queue_rw_mutex);
		return -ENOMEM;
	}

	if (copy_from_user(n->buffer, user_buffer, size))
	{
//		spin_unlock(&msg_queue_rw_lock);
//		up (&msg_queue_rw_mutex);
		return -EFAULT;
	}
	
	d->msg_queue_len++;
	n->len = size;
	
//	spin_unlock(&msg_queue_rw_lock);
//	up (&msg_queue_rw_mutex);
    
	return size;
}

static struct file_operations dummy_fops = {
	.owner = THIS_MODULE,
	.open = dummy_open,
	.release = dummy_release,
	.read = dummy_read,
	.write = dummy_write,
};

static int dummy_chrdev_init(void)
{
	/* alloc_chrdev_region() */
	//register_chrdev (0, ...) -- for dynamic allocation
	
	int i = 0;
	dev_t first_dev;
	int error = 0;
	if ((error = alloc_chrdev_region (&first_dev,
		DUMMY_MINOR, DUMMY_NUM_MINORS, DUMMY_MODULE_NAME)) != 0)
	{
		pr_err ("%s: alloc-chrdev_region failed: %d\n",
			__FUNCTION__, error);
		return error;
	}
	BUG_ON (MINOR (first_dev) + DUMMY_NUM_MINORS > MINORMASK);

	for (i = 0; i < DUMMY_NUM_MINORS; i++)
	{
		dummy_device_data[i].dev = MKDEV (MAJOR (first_dev),
						  MINOR (first_dev) + i);
	}


	return 0;
}

static int dummy_cdev_init(struct dummy_device_data *device_data)
{
	int error = 0;
	/* cdev_init() */
	cdev_init (&device_data->cdev, &dummy_fops);
	/* cdev_add() */
	error = cdev_add (&device_data->cdev, device_data->dev, 1);
	if (error)
	{
		pr_err ("%s: cdev_add failed: %d\n",
			__FUNCTION__, error);
		return error;
	}
	return 0;
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

	flag = 0;

	return 0;
}

static void dummy_exit(void)
{
	int i;

	for (i = 0; i < dummy_device_data[0].msg_queue_len; i++)
		vfree (dummy_device_data[0].msg_queue[0].buffer);

    for (i = 0; i < DUMMY_NUM_MINORS; i++)
	{
        cdev_del(&dummy_device_data[i].cdev);
    }
    unregister_chrdev_region(dummy_device_data[0].dev, DUMMY_NUM_MINORS);
    
	printk("%s: Bye\n", __FUNCTION__);
}

module_init(dummy_init);
module_exit(dummy_exit);
