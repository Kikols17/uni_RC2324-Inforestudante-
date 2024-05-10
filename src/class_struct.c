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


Class *create_classstruct(char *name, int size) {
    /* Allocate memory for a new class */
    Class *new_c = (Class *)malloc(sizeof(Class));
    if (new_c == NULL) {
        printf("!!!ERROR!!!\n-> Could not allocate memory for new_c.\n");
        return NULL;
    }

    strcpy(new_c->name, name);
    new_c->size = size;
    new_c->subscribed = 0;
    new_c->subscribed_ids = (int *)malloc(size * sizeof(int));
    if (new_c->subscribed_ids == NULL) {
        printf("!!!ERROR!!!\n-> Could not allocate memory for new_c->subscribed");
        free(new_c);
        return NULL;
    }
    return new_c;
}

int destroy_classstruct(Class *c) {
    /* Free memory allocated for a class */
    if (c == NULL) {
        printf("!!!ERROR!!!\n-> Cannot destroy a NULL class.\n");
        return -1;
    }
    free(c->subscribed_ids);
    free(c);
    return 0;
}

int addsub_classstruct(Class *c, int id) {
    /* Add a subscriber to a class */
    if (c == NULL) {
        printf("!!!ERROR!!!\n-> Cannot add a subscriber to a NULL class.\n");
        return -1;
    }
    if (c->subscribed == c->size) {
        printf("!!!ERROR!!!\n-> Cannot add more subscribers to class %s.\n", c->name);
        return -1;
    }
    c->subscribed_ids[c->subscribed] = id;
    c->subscribed++;
    return 0;
}

#endif