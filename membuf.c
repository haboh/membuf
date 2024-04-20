#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maxim Khabarov");
MODULE_DESCRIPTION("Character device with in-memory fixed-size buffer.");
MODULE_VERSION("0.1");

static ssize_t buf_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t buf_size_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count);

#define BUF_SIZE_DEFAULT (4096)

static int buf_size = 0;
static char *buffer = NULL;
static struct kobject *membuf_kobject;
static struct kobj_attribute foo_attribute = __ATTR(buf_size, 0660, buf_size_show, buf_size_store);

DECLARE_RWSEM(buffer_rw_sem);

module_param(buf_size, int, 0660);

static bool reallocate_buffer(int new_buf_size)
{
	char* new_buffer = (char *)kmalloc(new_buf_size, GFP_KERNEL);
        if (new_buffer == NULL)
        {
                return false;
        }
        memset(new_buffer, 0, new_buf_size);
        
	size_t copy_size = min(new_buf_size, buf_size);
	memcpy(new_buffer, buffer, copy_size);	
	
	kfree(buffer);
	buf_size = new_buf_size;
	buffer = new_buffer;
	
	return true;
}

static ssize_t buf_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        down_read(&buffer_rw_sem);
        ssize_t result;
        result = sprintf(buf, "%d\n", buf_size);
        up_read(&buffer_rw_sem);
        return result;
}

static ssize_t buf_size_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count)
{
        down_write(&buffer_rw_sem);
        ssize_t result = count;
	int new_buf_size;
        if (sscanf(buf, "%d", &new_buf_size) != 1)
        {
                result = 0;
                goto finish;
        }
        if (new_buf_size < 0)
        {
                result = 0;
                if (!reallocate_buffer(BUF_SIZE_DEFAULT)) {
                	result = -ENOMEM;
                }
                goto finish;
        }

        if (!reallocate_buffer(new_buf_size))
        {
        	result = -ENOMEM;
        	goto finish;
        }
finish:
        up_write(&buffer_rw_sem);
        return result;
}

static ssize_t membuf_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
        down_read(&buffer_rw_sem);
        ssize_t res;

	if (*off == buf_size)
	{
		res = 0;
		goto finish;
	}

        if (*off > buf_size) 
        {
                res = -ESPIPE;
                goto finish;
        }

        size_t count_bytes_to_copy = min((size_t)(buf_size - *off), len);
        if (copy_to_user(buf, buffer + *off, count_bytes_to_copy) != 0)
        {
                res = -EFAULT;
                goto finish;
        }

        *off += count_bytes_to_copy;
        res = count_bytes_to_copy;

finish:
        up_read(&buffer_rw_sem);
        return res;
}

static ssize_t membuf_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
        down_write(&buffer_rw_sem);
        ssize_t res;

	if (*off == buf_size)
	{
		res = 0;
		goto finish;
	}

        if (*off > buf_size)
        {
                res = -ENOSPC;
                goto finish;
        }
        
        size_t count_bytes_to_copy = min((size_t)(buf_size - *off), len);

        if (copy_from_user(buffer + *off, buf, count_bytes_to_copy) != 0)
        {
                res = -EFAULT;
                goto finish;
        }

        *off += count_bytes_to_copy;
        res = count_bytes_to_copy;

finish:
        up_write(&buffer_rw_sem);
        return res;
}

static dev_t dev = 0;
static struct cdev membuf_cdev;
static struct class *membuf_class;

static struct file_operations membuf_ops =
{
        .owner = THIS_MODULE,
        .read = membuf_read,
        .write = membuf_write,
};

static int __init mymodule_init(void)
{
        int error = 0;

        // create kobject with buf_size
        membuf_kobject = kobject_create_and_add("membuf", kernel_kobj);
        if (!membuf_kobject)
        {
                error = -ENOMEM;
                goto finish;
        }

        error = sysfs_create_file(membuf_kobject, &foo_attribute.attr);
        if (error)
        {
                pr_debug("failed to create the bufsize file in /sys/kernel/kobject_membuf \n");
                goto finish;
        }

        if (!reallocate_buffer(BUF_SIZE_DEFAULT))
        {
        	pr_debug("failed to allocate buffer in kernel memory \n");
        	error = -ENOMEM;
                goto finish;
	}

        // creating symbol device
        if ((error = alloc_chrdev_region(&dev, 0, 1, "membuf")) < 0)
        {
                pr_err("Error allocating major number\n");
                goto finish;
        }
        pr_info("CHRDEV load: Major = %d Minor = %d\n", MAJOR(dev), MINOR(dev));

        cdev_init(&membuf_cdev, &membuf_ops);
        if ((error = cdev_add(&membuf_cdev, dev, 1)) < 0)
        {
                pr_err("CHRDEV: device registering error\n");
                goto unregister_chrdev_region;
        }

        if (IS_ERR(membuf_class = class_create("membuf_class")))
        {
                error = -1;
                goto cdev_del;
        }

        if (IS_ERR(device_create(membuf_class, NULL, dev, NULL, "membuf_device")))
        {
                pr_err("membuf: error creating device\n");
                error = -1;
                goto class_destroy;
        }

        goto finish;

class_destroy:
        class_destroy(membuf_class);
cdev_del:
        cdev_del(&membuf_cdev);
unregister_chrdev_region:
        unregister_chrdev_region(dev, 1);
finish:
        return error;
}

static void __exit mymodule_exit(void)
{
        // removing kobject with buffer
        kobject_put(membuf_kobject);
        kfree(buffer);

        // removing character device
        device_destroy(membuf_class, dev);
        class_destroy(membuf_class);
        cdev_del(&membuf_cdev);
        unregister_chrdev_region(dev, 1);
}

module_init(mymodule_init);
module_exit(mymodule_exit);
