#ifndef CLASS_STRUCT_H
#define CLASS_STRUCT_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#define BASE_MULTICAST_ADDR 0xEF000001


#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

#ifndef N_USERS
#define N_USERS 124
#endif

typedef struct Class {
    char name[BUF_SIZE];
    int size;
    int subscribed;
    char subscribed_names[N_USERS][BUF_SIZE];
    struct sockaddr_in mutilcast_addr;
    int multicast_udpsocket;
} Class;


int create_classstruct(Class *c, int n, char *name, int size);
int destroy_classstruct(Class *c);
int addsub_classstruct(Class *c, char *username);
int sendmsg_classstruct(Class *c, char *message);

#endif