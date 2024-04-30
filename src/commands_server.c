#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands_server.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

// file pointer
extern FILE *config_file;


// Commands for both TCP client and UDP client
int login(struct User *user, char *user_name, char *password) {
    /* 
     * 
     * return:
     *      -1-> format error
     *      0-> user found
     *      1-> user not found
     *      2-> incorrect password
     */
    char *line = NULL;
    size_t len = BUF_SIZE;
    size_t size;
    int id = 0;

    char *possible_username, *possible_password, *possible_type;

    rewind(config_file);    // from beginning of file;
    while ( (size = getline(&line, &len, config_file))!=0 ) {
        line[size-1] = '\0';
        possible_username = strtok(line, ";");
        possible_password = strtok(NULL, ";");
        possible_type = strtok(NULL, ";");

        if ( possible_username==NULL  ||  possible_password==NULL  ||  possible_type==NULL ) {
            return -1;
        }

        if ( strcmp(user_name, possible_username)==0 ) {
            /* user name match */
            if ( strcmp(password, possible_password)==0 ) {
                /* password match */
                user->user_id = id;
                strcpy( user->name, possible_username);
                if ( strcmp(possible_type, "aluno")==0 ) {
                    user->type = ALUNO;
                } else if ( strcmp(possible_type, "professor")==0 ) {
                    user->type = PROFESSOR;
                } else if ( strcmp(possible_type, "administrator")==0 ) {
                    user->type = ADMINISTRADOR;
                } else {
                    // incorrect format
                    return -1;
                }
                return 0;
            } else {
                /* password mismatch */
                return 2;
            }
        }
        id++;
    }
    return 1;
}




// Commands for TCP client
int list_cmds_tcp(struct User *user, char *response) {
    sprintf(response+strlen(response), "List of TCP commands:\n"
                                       "  - LOGIN <user_name> <password>\n"
                                       "  -> After login:\n"
                                       "      - LIST_CLASSES\n"
                                       "      - LIST_SUBSCRIBED\n"
                                       "      - SUBSCRIBE_CLASS <class_name>\n"
                                       "  -> Professor permissions:\n"
                                       "      - CREATE_CLASS <class_name> <size>\n"
                                       "      - SEND <class_name> <text that server will send to subscribers>\n");
    return 0;
}

int list_classes(struct User *user, char *response) {
    // TODO
    printf("[TODO]: list classes for user \"%s\", and write to pointer %p.\n", user->name, response);
    return -1;
}

int list_subscribe(struct User *user, char *response) {
    // TODO
    printf("[TODO]: list subscribed for user \"%s\", and write to pointer %p.\n", user->name, response);
    return -1;
}

int subscribe_class(struct User *user, char *class_name) {
    // TODO
    printf("[TODO]: list subscribe user \"%s\", to class \"%s\".\n", user->name, class_name);
    return -1;
}

int create_class(struct User *user, char *class_name, int size) {
    // TODO
    printf("[TODO]: User \"%s\" create class \"%s\" with size %d.\n", user->name, class_name, size);
    return -1;
}

int send_message(struct User *user, char *class_name, char *message) {
    // TODO
    printf("[TODO]: User \"%s\" send message to class \"%s\", message \"%s\".\n", user->name, class_name, message);
    return -1;
}




// Commands for UDP client
int list_cmds_tcp(struct User *user, char *response) {
    sprintf(response+strlen(response), "List of TCP commands:\n"
                                       "  - LOGIN <user_name> <password>\n"
                                       "  -> After login:\n"
                                       "      - ADD_USER <username> <password> <type>\n"
                                       "      - DEL <username>\n"
                                       "      - LIST\n"
                                       "      - QUIT_SERVER\n");
    return 0;
}

int add_user(struct User *user, char *user_name, char *password, char* type) {
    // TODO
    printf("[TODO]: User \"%s\" created user with username \"%s\", password \"%s\" and type \"%s\".\n", user->name, user_name, password, type);
    return -1;
}

int del_user(struct User *user, char *user_name) {
    // TODO
    printf("[TODO]: User \"%s\" deleted user with username \"%s\".\n", user->name, user_name);
    return -1;
}

int list_users(struct User *user) {
    // TODO
    printf("[TODO]: User \"%s\" wants to list all users.\n", user->name);
    return -1;
}