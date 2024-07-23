#define err(msg){perror(msg); exit(1);}

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

# include "message_slot.h"

int main(int argc, char* args[]) {
    if(argc != 3) {
        err("Invalid number of arguments\n")
    }
    char* path = args[1];
    ssize_t channel_id = atoi(args[2]);
    char buffer[MESSAGE_LEN+1];
    buffer[MESSAGE_LEN] = '\0';

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        err("Error in file opening\n");
    }
    if(ioctl(fd, IOCTL_MSG_SLOT_CHANNEL, channel_id) < 0) {
        close(fd);
        err("Error in setting channel_id\n")
    }
    if(read(fd, buffer, MESSAGE_LEN) < 0) {
        err("Error in reading\n")
    }
    printf("%s\n", buffer);
    close(fd);
    exit(0);
}
