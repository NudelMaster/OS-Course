#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <linux/ioctl.h>

#define MAJOR_NUM 235

#define IOCTL_MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
# define MESSAGE_LEN 128
# define DEVICE_FILE_NAME "msg_slot_dev_file"
# define DEVICE_RANGE_NAME "msg_slot_dev"
#endif