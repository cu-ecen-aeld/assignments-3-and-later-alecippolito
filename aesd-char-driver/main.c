/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h> // for kmalloc, kfree
#include <linux/uaccess.h> // for copy_to_user, copy_from_user
#include "aesdchar.h"
#include "aesd_ioctl.h"

#define AESD_MAX_WRITE_COMMANDS 10

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Alec Ippolito"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

// Structure to hold each write command
struct aesd_buffer_entry {
    char *buff;
    size_t size;
};

struct aesd_dev {
    struct aesd_buffer_entry entry[AESD_MAX_WRITE_COMMANDS];
    struct cdev cdev;
    int head;
    int tail;
    struct mutex lock;  // Mutex for thread safety
};

static struct aesd_dev aesd_device;

struct aesd_circular_buffer {
    struct aesd_buffer_entry entry[AESD_MAX_WRITE_COMMANDS];
    int read_pos;
    int write_pos;
    spinlock_t lock;
};
// extern struct aesd_command aesd_buffer[AESD_MAX_WRITE_COMMANDS]; 

// Your ioctl handler function
static long aesd_char_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seek_data;
    int ret = 0;

    switch (cmd) {
        case AESDCHAR_IOCSEEKTO:
            // Copy the data from user-space to kernel-space
            if (copy_from_user(&seek_data, (struct aesd_seekto __user *)arg, sizeof(seek_data))) {
                return -EFAULT;  // Return error if user-to-kernel copy fails
            }

            // Check if the provided write command index is within valid range
            if (seek_data.write_cmd >= AESD_MAX_WRITE_COMMANDS || seek_data.write_cmd_offset >= aesd_device.entry[seek_data.write_cmd].size) {
                return -EINVAL;  // Return error for invalid index or offset
            }

            // Calculate the position we need to seek to
            unsigned int pos = 0;

            // Calculate the byte offset for the requested seek command and offset
            int i;
            for (i=0; i < seek_data.write_cmd; i++) {
                // pos += aesd_buffer[i].size;  // Add up the size of previous commands
            }

            // pos += seek_data.write_cmd_offset;  // Add the offset within the current command

            // Set the file pointer to the desired position
            file->f_pos = pos;

            break;

        default:
            ret = -ENOTTY;  // Return error for unsupported commands
            break;
    }

    return ret;
}

// Initialize the circular buffer
struct aesd_circular_buffer aesd_buffer = {
    .read_pos = 0,
    .write_pos = 0,
    .lock = __SPIN_LOCK_UNLOCKED(aesd_buffer.lock),
};

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t new_pos = 0;
    size_t total_size = 0;
    int i;

    mutex_lock(&aesd_device.lock);
    for (i = 0; i < AESD_MAX_WRITE_COMMANDS; i++) {
        total_size += aesd_device.entry[i].size;
    }
    mutex_unlock(&aesd_device.lock);

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = filp->f_pos + offset;
            break;
        case SEEK_END:
            new_pos = total_size + offset;
            break;
        default:
            return -EINVAL;
    }

    if (new_pos < 0 || new_pos > total_size)
        return -EINVAL;

    filp->f_pos = new_pos;
    return new_pos;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = &aesd_device; // Store device data
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    size_t bytes_read = 0;
    int idx = 0;
    int total_bytes = 0;

    mutex_lock(&aesd_device.lock);

    // Start from the tail of the buffer and read the most recent 10 commands
    for (idx = aesd_device.tail; idx != aesd_device.head; idx = (idx + 1) % AESD_MAX_WRITE_COMMANDS) {
        total_bytes += aesd_device.entry[idx].size;
    }

    // Return only the requested number of bytes
    if (count > total_bytes) {
        count = total_bytes;
    }

    for (idx = aesd_device.tail; idx != aesd_device.head && bytes_read < count; idx = (idx + 1) % AESD_MAX_WRITE_COMMANDS) {
        size_t remaining = count - bytes_read;
        size_t size_to_copy = aesd_device.entry[idx].size;

        if (remaining < size_to_copy) {
            size_to_copy = remaining;
        }

        if (copy_to_user(buf + bytes_read, aesd_device.entry[idx].buff, size_to_copy)) {
            mutex_unlock(&aesd_device.lock);
            return -EFAULT;
        }

        bytes_read += size_to_copy;
    }

    mutex_unlock(&aesd_device.lock);
    
    return bytes_read;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
        size_t bytes_written = 0;
    char *kern_buf = NULL;
    size_t write_size = count;
    
    // Allocate memory for the write operation
    kern_buf = kmalloc(write_size, GFP_KERNEL);
    if (!kern_buf) {
        return -ENOMEM;
    }

    // Copy data from user space to kernel space
    if (copy_from_user(kern_buf, buf, write_size)) {
        kfree(kern_buf);
        return -EFAULT;
    }

    // Lock access to buffer to ensure thread safety
    mutex_lock(&aesd_device.lock);

    // Store the write command in the circular buffer
    aesd_device.entry[aesd_device.head].buff = kern_buf;
    aesd_device.entry[aesd_device.head].size = write_size;
    aesd_device.head = (aesd_device.head + 1) % AESD_MAX_WRITE_COMMANDS;

    // If buffer is full, free memory for the oldest write
    if (aesd_device.head == aesd_device.tail) {
        kfree(aesd_device.entry[aesd_device.tail].buff);
        aesd_device.tail = (aesd_device.tail + 1) % AESD_MAX_WRITE_COMMANDS;
    }

    mutex_unlock(&aesd_device.lock);

    return count;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    int result;
    dev_t dev;

    // Allocate major and minor numbers
    result = alloc_chrdev_region(&dev, 0, 1, "aesdchar");
    if (result < 0) {
        printk(KERN_WARNING "aesdchar: can't allocate major number\n");
        return result;
    }

    // Register the character device
    cdev_init(&aesd_device.cdev, &aesd_fops);
    aesd_device.cdev.owner = THIS_MODULE;
    result = cdev_add(&aesd_device.cdev, dev, 1);
    if (result) {
        printk(KERN_WARNING "aesdchar: can't add device\n");
        unregister_chrdev_region(dev, 1);
        return result;
    }

    mutex_init(&aesd_device.lock);
    printk(KERN_INFO "aesdchar: module loaded\n");

    return 0;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * Cleanup AESD specific portions here
     */
    int i; 
    for (i = 0; i < AESD_MAX_WRITE_COMMANDS; i++) {
        if (aesd_device.entry[i].buff) {
            kfree(aesd_device.entry[i].buff);
        }
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

