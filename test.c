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

#define MAX_LENGTH_MSG 512 //511 + '\0'

static DEFINE_MUTEX(read_lock);
static DEFINE_MUTEX(write_lock);

int openCount = 0;
int closeCount = 0;

struct dummy_device_data {
	struct cdev cdev;
	dev_t dev;
};

struct fifo_cell 
{
    struct fifo_cell* next_cell;
    char msg[MAX_LENGTH_MSG];
};

struct dummy_fifo 
{
    struct fifo_cell* start;
    struct fifo_cell* end;
} fifo;

/**
 * Если очередь пуста, то вернет 1, иначе 0
 */
int isFifoEmpty (struct dummy_fifo* fifo){
    if ( (fifo->start == NULL) && (fifo->end == NULL))
    {
	return 1;
    }else
	return 0;
}

void AddMsgToFifo( char msg[MAX_LENGTH_MSG - 1], struct dummy_fifo* fifo){
    struct fifo_cell *new_cell = kmalloc( sizeof( struct fifo_cell), GFP_KERNEL);
    new_cell -> next_cell = NULL;
    strcpy( new_cell->msg, msg);

    if( isFifoEmpty( fifo) )
    {
	fifo->start = new_cell;
	fifo->end = new_cell;
    }else
    {
	new_cell->next_cell = fifo->end;
	fifo->end = new_cell;
    }

}

/**
 * Вытаскивает сообщение из очереди и печатает его. Если очередь пуста, то вернет 1
 * UPD: Странное поведние, если возвращает не 0. Так что пусть всегда вернет 0
 */
int GetMsgFromFifo( struct dummy_fifo* fifo)
{
    if ( isFifoEmpty( fifo))
    {
	printk(KERN_INFO "[ALERT] Fifo is empty. Nothing to read\n");
	return 0;
    }
    /* Случай, когда 1 элемент в очереди */
    if (fifo->start == fifo->end)
    {
	    printk(KERN_INFO "[MESSAGE] %s\n", fifo->start->msg);
	    kfree(fifo->start);
	    fifo->start = NULL;
	    fifo->end = NULL;
	    return 0;
    }else
    /* Случай, когда в очереди >1 элемента */
    {
	struct fifo_cell *cell = fifo->start;
	struct fifo_cell *prev_cell = fifo->end;

	while(prev_cell->next_cell != cell)
	{
	    prev_cell = prev_cell->next_cell;
	}

	printk(KERN_INFO "[MESSAGE] %s\n", cell->msg);
	kfree(cell);
	fifo->start = prev_cell;
	prev_cell->next_cell = NULL;
	return 0;
    }
}


/* Буфер и размер данных для записи в него */
static char driver_buffer[MAX_LENGTH_MSG];
static unsigned long driver_buffer_size = 0;


static struct kfifo test;
static struct dummy_device_data dummy_device_data[DUMMY_NUM_MINORS];

ssize_t dummy_read (struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copied;

	if (mutex_lock_interruptible(&read_lock))
		return -ERESTARTSYS; //Необходимо для перезапуска процесса записи, если mutex кем-то захвачен
	
	ret = GetMsgFromFifo( &fifo);

	mutex_unlock(&read_lock);

	return ret;
}
 
ssize_t dummy_write (struct file *file, const char __user *buffer, size_t len, loff_t *off)
{
	if (mutex_lock_interruptible(&write_lock))
		return -ERESTARTSYS; //Необходимо для перезапуска процесса записи, если mutex кем-то захвачен


	if ( len > MAX_LENGTH_MSG - 1 )	{
		driver_buffer_size = MAX_LENGTH_MSG - 1;
	}
	else	{
		driver_buffer_size = len;
	}
	
	if ( copy_from_user(driver_buffer, buffer, driver_buffer_size) ) {
		return -EFAULT;
	}

	AddMsgToFifo( driver_buffer, &fifo);

	printk(KERN_INFO "[INFO] writed %lu bytes\n", driver_buffer_size);

	mutex_unlock(&write_lock);
	
	return driver_buffer_size;
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
	fifo.start = NULL;
	fifo.end = NULL;

	return 0;
}

static void dummy_exit(void)
{
	printk(KERN_ALERT "[EXITING]\n");
	kfree(&fifo);
}


module_init(dummy_init);
module_exit(dummy_exit);
