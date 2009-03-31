#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "fdpass.h"

unsigned int rcvbuf = 16*1024*1024;
unsigned int arg_length = 128;
unsigned int mountflags = MS_NODEV | MS_NOSUID;

void receive_argument(int control_channel_fd, char *source) {
    int received;
    received=recv(control_channel_fd, source, arg_length, 0);
    if (received<arg_length) {
        printf("Error receiving arguments over the control buffer\n");
        exit(1);
    }
}

int set_magic_fd (char *data, int new_fd) {
    char *ptr;
    int fd;

    data[arg_length-1]='\0';
    ptr = strstr(data,"fd=");
    if (!ptr)
        return -1;

    // Found two fd= expressions
    if (strstr(ptr+3,"fd="))
        return -1;

    if (*(ptr+3)!='\0') {
        char *new_data = (char *) malloc(arg_length);
        char *head = (char *) malloc(arg_length);
        char *tail = (char *) malloc(arg_length);
        sscanf(ptr+3,"%d",&fd);
        strncpy(head, data, ptr - data);
        tail = strchr(ptr+3,',');
        sprintf(new_data,"%sfd=%d,%s",head,new_fd,tail);
        strcpy(data,new_data);
        free(new_data);
        free(head);
        free(tail);
        return fd;
    }
    else
        return -1;
}

void check_source(char *source) {
    source[arg_length-1]='\0';
    if (strchr(source,'/') || strstr(source,"..")) {
        printf("Tried mounting with source = %s\n", source);
        exit(1);
    }
}

void check_target(char *target) {
    target[arg_length-1]='\0';
    if (strstr(target,"..")) {
        printf("Tried mounting with target = %s\n", target);
        exit(1);
    }
}

void check_fstype(char *filesystemtype) {
    if (strncmp(filesystemtype,"fuse",4)) {
        printf("Tried mounting filesystem type %s\n", filesystemtype);
    }
}

int main(int argc, char *argv[]) {
    int control_channel_fd, magic_fd, mount_fd;
    char source[128],target[128],filesystemtype[128],data[128],slice_target[256];

    int received;

    if (argc < 3) {
        printf("This script is called by vsys.\n");
        exit(1);
    }

    char *slice_name = argv[1];
    
    sscanf(argv[2],"%d", &control_channel_fd);

    if (control_channel_fd <3 || control_channel_fd > 1023) {
        printf ("Got control_channel_fd = %d\n", control_channel_fd);
        exit(1);
    }

    receive_argument(control_channel_fd, source);
    receive_argument(control_channel_fd, target);
    receive_argument(control_channel_fd, filesystemtype);
    receive_argument(control_channel_fd, data);

    mount_fd = receive_fd (control_channel_fd);

    if (mount_fd < 2) {
        printf("mount_fd = %d\n", mount_fd);
        exit(1);
    }

    set_magic_fd(data, mount_fd);


    check_source(source);
    check_target(target);
    check_fstype(filesystemtype);

    sprintf(slice_target,"/vservers/%s/%s", slice_name, target);

    if (!mount(source, slice_target, filesystemtype, mountflags, data)) {
        send_fd(control_channel_fd, mount_fd);
    }
    else {
        printf ("Error executing mount\n");
        exit(1);
    }

    return 0;
}
