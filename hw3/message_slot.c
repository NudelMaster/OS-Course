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
#include "message_slot.h"
MODULE_LICENSE("GPL");


// =============== data structures ==========================

typedef struct node {
    ssize_t channel_id;
    char *msg;
    int msg_len;
    struct channel *next;
}channel;

typedef struct device {
    int minor;
    channel* channels;
}Device;

static Device devices[256]; // 256 minors

// ===================== CHANNEL GET/SET =======================================

static channel* getOrCreate_Channel(Device* dev, ssize_t id) {
    channel* head;
    channel *curr;
    channel *prev;
    head = dev->channels;
    // in case of empty list
    if(head == NULL) {
        head = (channel *)kmalloc(sizeof(channel), GFP_KERNEL);
        if(head == NULL) {
            kfree(head);
            return NULL;
        }
        head->channel_id = id;
        head->msg = NULL;
        head->msg_len = 0;
        head->next = NULL;
        return head;
    }

    // if list isn't empty then there is at least one channel as the head


    curr = head;
    prev = curr;
    // assuming that list has at least one node
    while(curr != NULL) {
        if(curr->channel_id == id) {
            return curr;
        }
        prev = curr;
        curr = (channel *)curr->next;
    }
    // if we haven't found an id and reached curr == NULL
    prev->next = kmalloc(sizeof(channel), GFP_KERNEL);
    prev = (channel *)prev->next;
    prev->channel_id = id;
    prev->msg = NULL;
    prev->msg_len = 0;
    prev->next = NULL;
    return prev;
}

// ======================= MEMORY CLEANUP =========================================

static void free_slot_mem(channel* head) {
    channel *curr, *next;

    if (head == NULL) {
        return;
    }
    curr = head;
    while(curr != NULL) {
        kfree(curr->msg);
        next = (channel *)curr->next;
        kfree(curr);
        curr = next;
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
    int minor = iminor(inode);
    printk("Invoking device_open(%p)\n", file);


    if(devices[minor].minor < 0) {
        printk("Creating msg slot for minor: %d\n", minor);
        devices[minor].minor = minor;
        devices[minor].channels = NULL;
    }
    return 0;
}
static int device_release(struct inode* inode, struct file* file) {

    printk("Releasing open instance of file with minor: %d, and channel: %lu\n", iminor(inode), (unsigned long)file->private_data);
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
    if(length > MESSAGE_LEN) {
        return -EMSGSIZE;
    }
    channel_id = (ssize_t)file->private_data;
    minor = iminor(file->f_inode);

    // assuming minor size are in range since using chregister
    ch = getOrCreate_Channel(&devices[minor], channel_id);
    ch->msg = kmalloc(MESSAGE_LEN + 1, GFP_KERNEL);
    if(ch->msg == NULL) {
        printk(KERN_WARNING "No message has been sent\n");
        return -EFAULT;
    }

    printk(KERN_INFO "Invoking writing into device file with minor %d,  channel_id: the msg: :%ld%s\n", minor, channel_id, buffer);
    err = copy_from_user(ch->msg, buffer, length);
    if(!err) {
        printk(KERN_WARNING "Error in copying from user");
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
        return -EINVAL;
    }
    channel_id = (ssize_t)file->private_data;
    minor = iminor(file->f_inode);
    // assuming minor size are in range since using chregister
    ch = getOrCreate_Channel(&devices[minor], channel_id);
    // checking if it has a msg
    if(ch->msg == NULL) {
        return -EWOULDBLOCK;
    }
    if(ch->msg_len > MESSAGE_LEN) {
        return -ENOSPC;
    }

    err = copy_to_user(buffer, ch->msg, ch->msg_len);
    if(!err) {
        return -EFAULT;
    }
    return ch->msg_len;
}

static long device_ioctl(struct file* file,
    unsigned int ioctl_command_id,
    unsigned long ioctl_param) {

    printk(KERN_INFO "device_ioctl called\n");
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
    int rc;
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