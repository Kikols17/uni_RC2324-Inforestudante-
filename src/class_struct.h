#ifndef CLASS_STRUCT_H
#define CLASS_STRUCT_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

typedef struct Class {
    char name[BUF_SIZE];
    int size;
    int subscribed;
    char (*subscribed_names)[BUF_SIZE];
} Class;


int create_classstruct(Class *c, char *name, int size);
int destroy_classstruct(Class *c);
int addsub_classstruct(Class *c, char *username);

#endif