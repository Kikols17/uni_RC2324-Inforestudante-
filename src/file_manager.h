#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif


int file_checkintegrity(char *filename);
int file_finduser(char *filename, char *username, char *password, char *type);
int file_adduser(char *filename, char *username, char *password, char *type);
int file_removeuser(char *filename, char *username);
int file_listusers(char *filename, char *response);




#endif