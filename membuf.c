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
MODULE_AUTHOR("maxim");
MODULE_DESCRIPTION("none");
MODULE_VERSION("0.1");

static ssize_t buf_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t buf_size_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count);

#define BUF_SIZE_DEFAULT (4096)

static int buf_size = BUF_SIZE_DEFAULT;
static char *buffer = NULL;
static struct kobject *membuf_kobject;
static struct kobj_attribute foo_attribute = __ATTR(buf_size, 0660, buf_size_show, buf_size_store);

// TODO: change with rw-version
DEFINE_MUTEX(buffer_lock);

module_param(buf_size, int, 0660);

static void reallocate_buffer(void)
{
        kfree(buffer);
        buffer = (char *)kmalloc(buf_size, GFP_KERNEL);
        if (buffer == NULL)
        {
                panic("kmalloc returned an error\n");
        }
        memset(buffer, 0, buf_size);
}

static ssize_t buf_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        mutex_lock(&buffer_lock);
        ssize_t result;
        result = sprintf(buf, "%d\n", buf_size);
        mutex_unlock(&buffer_lock);
        return result;
}

static ssize_t buf_size_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count)
{
        mutex_lock(&buffer_lock);
        ssize_t result = count;
        if (sscanf(buf, "%d", &buf_size) != 1)
        {
                result = 0;
                goto finish;
        }
        if (buf_size < 0)
        {
                result = 0;
                buf_size = BUF_SIZE_DEFAULT;
                reallocate_buffer();
                goto finish;
        }

        reallocate_buffer();
finish:
        mutex_unlock(&buffer_lock);
        return result;
}

static ssize_t membuf_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
        mutex_lock(&buffer_lock);
        ssize_t res;

        if (*off >= buf_size) 
        {
                res = -EFAULT;
                goto finish;
        }

        size_t count_bytes_to_copy =  (buf_size - *off) < len ? (buf_size - *off) : len;
        if (copy_to_user(buf, buffer + *off, count_bytes_to_copy) != 0)
        {
                res = -EFAULT;
                goto finish;
        }

        *off += count_bytes_to_copy;
        res = count_bytes_to_copy;

finish:
        mutex_unlock(&buffer_lock);
        return res;
}

static ssize_t membuf_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
        mutex_lock(&buffer_lock);
        ssize_t res;

        if (*off >= buf_size)
        {
                res = -EFAULT;
                goto finish;
        }

        
        size_t count_bytes_to_copy =  (buf_size - *off) < len ? (buf_size - *off) : len;

        if (copy_from_user(buffer + *off, buf, count_bytes_to_copy) != 0)
        {
                res = -EFAULT;
                goto finish;
        }

        *off += count_bytes_to_copy;
        res = count_bytes_to_copy;

finish:
        mutex_unlock(&buffer_lock);
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
        membuf_kobject = kobject_create_and_add("kobject_membuf", kernel_kobj);
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

        reallocate_buffer();

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
