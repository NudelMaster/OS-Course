#define err(msg){perror(msg); exit(1);}

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

# include "message_slot.h"

int main(int argc, char* args[]) {
    if (argc != 4) {
        err("Invalid Number of arguments\n")
    }
    char* path = args[1];
    ssize_t channel_id = atoi(args[2]);
    char* msg = args[3];

    int fd = open(path, O_RDWR);
    if(fd < 0) {
        err("Error in opening file\n")
    }
    int set_id = ioctl(fd, IOCTL_MSG_SLOT_CHANNEL, channel_id);
    if(set_id < 0) {
        close(fd);
        err("Error in setting channel idn");
    }

    if(write(fd, msg, strlen(msg)) < 0) {
        close(fd);
        err("Error in writing msg %d\n")
    }
    printf("Closing fd\n");
    close(fd);
    exit(0);
}