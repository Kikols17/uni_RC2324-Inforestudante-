#ifndef COMMANDS_SERVER_H
#define COMMANDS_SERVER_H

#include <stdio.h>

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif


enum User_type { ALUNO, PROFESSOR, ADMINISTRADOR };

typedef struct User {
    int user_id;
    char name[BUF_SIZE];
    enum User_type type;
}User;


// Commands for both TCP client and UDP client
int login(struct User *user, char *user_name, char *password);
int logout(struct User *user, char *response);

// Commands for TCP client
int list_cmds_tcp(char *response);
int list_classes(char *response);
int list_subscribe(struct User *user, char *response);
int subscribe_class(struct User *user, char *class_name, char *response);
int create_class(char *class_name, int size, char *response);
int send_message(struct User *user, char *class_name, char *message);


int list_cmds_udp(char *response);
int add_user(struct User *user, char *user_name, char *password, char* type, char *response);
int del_user(struct User *user, char *user_name, char *response);
int list_users(char *response);


#endif