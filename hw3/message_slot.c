#undef KERNEL
#define KERNEL

#undef MODULE
#define MODULE

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include<linux/kernel.h>
#include<linux/fs.h>
#include<linux/uaccess.h>
#include<linux/string.h>
#include "message_slot.h"
MODULE_LICENSE("GPL");


// =============== data structures ==========================

typedef struct node {
    ssize_t channel_id;
    char msg[MESSAGE_LEN];
    int msg_len;
    struct node *next;
}channel;

typedef struct device {
    int minor;
    channel* channels;
}Device;

static Device devices[256]; // 256 minors

// ===================== CHANNEL GET/SET =======================================

static channel* getOrCreate_Channel(Device* dev, ssize_t id) {
    channel* head;
    channel *prev;
    head = dev->channels;
    // in case of empty list
    if(!head) {
        printk("Device desn't have channels yet, creating new channel head: %lu \n", id);
        head = kmalloc(sizeof(channel), GFP_KERNEL);
        if(head == NULL) {
            printk("Failed to allocate memory \n");
            return NULL;
        }
        head->channel_id = id;
        memset(head->msg, 0, sizeof(head->msg));
        head->msg_len = 0;
        head->next = NULL;
        // assigning head of this device channel
        dev->channels = head;
        printk(KERN_INFO "Created new head channel %lu\n", id);
        return head;
    }

    // list isn't empty
    while(head) {
        printk("curr channel id is %ld \n", head->channel_id);
        if(head->channel_id == id) {
            printk(KERN_INFO "found channel %lu \n", id);
            return head;
        }
        prev = head;
        head = head->next;
    }
    // if not found, create new channel
    head = kmalloc(sizeof(channel), GFP_KERNEL);
    if(!head){
        printk(KERN_WARNING "Failed to allocate memory\n");
        return NULL;
    }
    head->channel_id = id;
    head->msg_len = 0;
    memset(head->msg, 0, sizeof(head->msg));
    head->next = NULL;

    // add new node to the list
    prev->next = head;

    printk(KERN_INFO "created new channel %lu\n", id);
    return head;

}

// ======================= MEMORY CLEANUP =========================================

static void free_slot_mem(channel* head) {


    channel *next;
    if (head == NULL) {
        return;
    }
    while(head != NULL) {
        kfree(head->msg);
        next = head->next;
        kfree(head);
        head = next;
    }
}
static void free_devices_mem(void) {
    int i;
    for(i = 0; i < 256;i++) {
        free_slot_mem(devices[i].channels);
    }
}

// ====================== DEVICE FUNCTIONS =============================

static int device_open(struct inode* inode, struct file* file) {
    int minor;
    minor = iminor(inode);
    printk(KERN_INFO "Invoking device_open for file: %p, with minor: %d\n", file, minor);

    if(devices[minor].minor < 0) {
        printk(KERN_INFO "Creating msg slot for minor: %d\n", minor);
        devices[minor].minor = minor;
        devices[minor].channels = NULL;
    }
    return 0;
}
static int device_release(struct inode* inode, struct file* file) {

    printk("Releasing open instance of file with minor: %d, and channel: %lu \n", iminor(inode), (ssize_t)file->private_data);
    file->private_data = NULL;
    return 0;

}

static ssize_t device_write(struct file* file,
                        const char __user* buffer,
                        size_t length,
                        loff_t* offset) {
    int err, minor;
    ssize_t channel_id;
    channel* ch;
    if(file->private_data == NULL) {
        printk(KERN_WARNING "No channel set\n");
        return -EINVAL;
    }
    if(length == 0 || length > MESSAGE_LEN) {
		printk("Provided length is invalid \n");
        return -EMSGSIZE;
    }
    channel_id = (ssize_t)file->private_data;
    minor = iminor(file->f_inode);

    // assuming minor size are in range since using chregister
    printk("getting or creating node for channel id %lu \n", channel_id);
    ch = getOrCreate_Channel(&devices[minor], channel_id);



    printk(KERN_INFO "Invoking writing into device file with minor: %d, channel_id: %lu, the msg:: %s \n", minor, channel_id, buffer);
    err = copy_from_user(ch->msg, buffer, length);
    if(err != 0) {
        printk(KERN_WARNING "Error in copying from user \n");
        return -EFAULT;
    }
    ch->msg_len = length;

    return length;
}

static ssize_t device_read(struct file* file,
                        char __user* buffer,
                        size_t length,
                        loff_t* offset) {
    int err, minor;
    ssize_t channel_id;
    channel *ch;
    if(file->private_data == NULL) {
        printk(KERN_WARNING "error, the file doesn't point to channel \n");
        return -EINVAL;
    }
    channel_id = (ssize_t)file->private_data;
    minor = iminor(file->f_inode);
    // assuming minor size are in range since using chregister
    printk("Reading from device with minor : %d, and channel_id %ld\n",minor, channel_id);
    ch = getOrCreate_Channel(&devices[minor], channel_id);
    // checking if it has a msg
    if(ch->msg_len == 0) {
        printk(KERN_WARNING "Error, no message exists on channel\n");
        return -EWOULDBLOCK;
    }
    if(ch->msg_len > length) {
		printk(KERN_WARNING "Provided buffer length is too small to contain the previous written message \n");
        return -ENOSPC;
    }
    err = copy_to_user(buffer, ch->msg, ch->msg_len);
    if(err != 0) {
        printk(KERN_WARNING "error in copying\n");
        return -EFAULT;
    }
    return ch->msg_len;
}

static long device_ioctl(struct file* file,
    unsigned int ioctl_command_id,
    unsigned long ioctl_param) {

    printk(KERN_INFO "device_ioctl called for minor: %d, passing channel: %lu \n", iminor(file->f_inode), ioctl_param);
    if(ioctl_command_id != IOCTL_MSG_SLOT_CHANNEL || ioctl_param == 0) {
        return -EINVAL;
    }
    // setting file discriptor channel id
    file->private_data = (void*)ioctl_param;
    return 0;
}

// ======================== DEVICE SETUP =============================
struct file_operations FOPS = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,


};

static int __init msg_slot_init(void) {
    int rc = -1;
    int i;
    printk(KERN_INFO "Initializing message slot module\n");
    rc = register_chrdev(MAJOR_NUM, DEVICE_FILE_NAME, &FOPS);
    if (rc < 0) {
        printk(KERN_ALERT "%s registration failed for %d\n", DEVICE_FILE_NAME, MAJOR_NUM);
        return rc;
    }
    for (i = 0; i < 256; i++) {
        devices[i].minor = -1;
        devices[i].channels = NULL;
    }
    printk(KERN_INFO "Registration is successful\n");

    return 0;
}
static void __exit msg_slot_cleanup(void) {
    printk("Cleaning up.\n");
    // free allocated memory
    free_devices_mem();
    // unregister the device, should always succeed
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}
module_init(msg_slot_init);
module_exit(msg_slot_cleanup);
