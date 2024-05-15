#ifndef CLASS_STRUCT_C
#define CLASS_STRUCT_C

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "class_struct.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif


int create_classstruct(Class *c, char *name, int size) {
    /* Allocate memory for a new class */
    if (c->name[0] != '\0') {
        printf("!!!ERROR!!!\n-> Cannot create a class in a non-empty struct.\n");
        return -1;
    }
    if (size <= 0) {
        printf("!!!ERROR!!!\n-> Cannot create a class with size <= 0.\n");
        return -2;
    }
    strcpy(c->name, name);
    c->size = size;
    c->subscribed = 0;
    c->subscribed_names = (char (*)[BUF_SIZE])malloc(size * sizeof(char [BUF_SIZE]));
    if (c->subscribed_names == NULL) {
        printf("!!!ERROR!!!\n-> Could not allocate memory for class %s.\n", name);
        return -3;
    }
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
    free(c->subscribed_names);
    c->subscribed_names = NULL;
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

#endif