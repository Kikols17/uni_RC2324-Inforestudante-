#ifndef CLASS_STRUCT_C
#define CLASS_STRUCT_C

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "class_struct.h"


#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif


int create_classstruct(Class *c, int n, char *name, int size) {
    /* Allocate memory for a new class */
    if (c->name[0] != '\0') {
        printf("!!!ERROR!!!\n-> Cannot create a class in a non-empty struct.\n");
        return -1;
    }
    if (size <= 0) {
        printf("!!!ERROR!!!\n-> Cannot create a class with size <= 0.\n");
        return -2;
    } else if (size > N_USERS) {
        printf("!!!ERROR!!!\n-> Cannot create a class with size > N_USERS: %d.\n", N_USERS);
        return -3;
    }
    strcpy(c->name, name);
    c->size = size;
    c->subscribed = 0;
    for (int i=0; i<size; i++) {
        // reset all subscribed names
        c->subscribed_names[i][0] = '\0';
    }
    bzero(&c->mutilcast_addr, sizeof(c->mutilcast_addr));
    c->mutilcast_addr.sin_family = AF_INET;
    c->mutilcast_addr.sin_addr.s_addr = htonl(BASE_MULTICAST_ADDR + n);
    c->mutilcast_addr.sin_port = htons(5000 + c->mutilcast_addr.sin_addr.s_addr%1000);
    //printf("[DEBUG] SUPOSED PORT: %d\n", 5000 + c->mutilcast_addr.sin_addr.s_addr%1000);
    return 0;
}

int destroy_classstruct(Class *c) {
    /* Free memory allocated for a class */
    if (c->name[0] == '\0') {
        printf("!!!ERROR!!!\n-> Cannot free memory for an empty class.\n");
        return -1;
    }
    c->name[0] = '\0';
    c->size = -1;
    c->subscribed = 0;
    for (int i=0; i<c->size; i++) {
        // reset all subscribed names
        c->subscribed_names[i][0] = '\0';
    }
    bzero(&c->mutilcast_addr, sizeof(c->mutilcast_addr));
    return 0;
}

int addsub_classstruct(Class *c, char *username) {
    /* Add a subscriber to a class */
    if (c == NULL) {
        printf("!!!ERROR!!!\n-> Cannot add a subscriber to a NULL class.\n");
        return -2;
    }
    if (c->subscribed == c->size) {
        printf("!!!ERROR!!!\n-> Cannot add more subscribers to class %s.\n", c->name);
        return -1;
    }
    for (int i=0; i<c->subscribed; i++) {
        if ( strcmp(c->subscribed_names[i], username)==0) {
            printf("!!!ERROR!!!\n-> User \"%s\" is already subscribed to class %s.\n", username, c->name);
            return 1;
        }
    }
    strcpy(c->subscribed_names[c->subscribed], username);
    c->subscribed++;
    return 0;
}

int sendmsg_classstruct(Class *c, char *message) {
    if (c->name[0] == '\0') {
        printf("!!!ERROR!!!\n-> Cannot send message to an empty class.\n");
        return -1;
    }
    if (c == NULL) {
        printf("!!!ERROR!!!\n-> Cannot send message to a NULL class.\n");
        return -2;
    }
    if (c->mutilcast_addr.sin_addr.s_addr == 0) {
        printf("!!!ERROR!!!\n-> Cannot send message to class \"%s\" with invalid multicast address.\n", c->name);
        return -3;
    }

    c->multicast_udpsocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(c->multicast_udpsocket < 0){
        printf("!!!ERROR!!!\n-> Cannot create socket for class \"%s\".\n", c->name);
        close(c->multicast_udpsocket);
        return -4;
    }

    int ttl = 6;
    if( setsockopt(c->multicast_udpsocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))<0 ){
        printf("!!!ERROR!!!\n-> Cannot set TTL for class \"%s\".\n", c->name);
        close(c->multicast_udpsocket);
        return -5;
    }

    if(sendto(c->multicast_udpsocket, message, strlen(message)+1, 0, (struct sockaddr *)&(c->mutilcast_addr), sizeof(c->mutilcast_addr)) < 0){
        printf("!!!ERROR!!!\n-> Cannot send message to class \"%s\".\n", c->name);
        close(c->multicast_udpsocket);
        return -6;
    }

    sleep(1);
    close(c->multicast_udpsocket);
    return 0;
}

#endif