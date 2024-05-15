#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include "commands_server.h"
#include "class_struct.h"
#include "file_manager.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

extern int n_classes;

// file stuff
extern char config_file_path[BUF_SIZE];
extern sem_t *config_sem;

// class stuff
extern struct Class *classes;
extern sem_t *class_sem;

// Commands for both TCP client and UDP client
int login(struct User *user, char *user_name, char *password) {
    /* Logs in user based on user_name and password from "config_file"
     *
     * return:
     *      -1-> format error
     *      0-> user found
     *      1-> user not found
     *      2-> incorrect password
     */

    int ret;
    char type[BUF_SIZE];
    ret = file_finduser(config_file_path, user_name, password, type);
    if (ret == 0) {
        user->user_id = 0; // TODO: user_id
        strcpy(user->name, user_name);
        printf("Type: \"%s\"\n", type);
        if (strcmp(type, "aluno") == 0) {
            user->type = ALUNO;
        } else if (strcmp(type, "professor") == 0) {
            user->type = PROFESSOR;
        } else if (strcmp(type, "administrator") == 0) {
            user->type = ADMINISTRADOR;
        } else {
            // incorrect format
            printf("pila\n");
            return -1;
        }
        return 0;
    } else {
        return ret;
    }
    
    /*
    char *line = NULL;
    size_t len = BUF_SIZE;
    size_t size;
    int id = 0;

    char *possible_username, *possible_password, *possible_type;

    sem_wait(config_sem);
    rewind(config_file); // from beginning of file;
    while ((size = getline(&line, &len, config_file)) != 0) {
        line[size - 1] = '\0';
        possible_username = strtok(line, ";");
        possible_password = strtok(NULL, ";");
        possible_type = strtok(NULL, ";");

        if (possible_username == NULL || possible_password == NULL || possible_type == NULL) {
            // incorrect format
            sem_post(config_sem);
            return -1;
        }

        if (strcmp(user_name, possible_username) == 0) {
            / user name match /
            if (strcmp(password, possible_password) == 0) {
                / password match /
                user->user_id = id;
                strcpy(user->name, possible_username);
                if (strcmp(possible_type, "aluno") == 0) {
                    user->type = ALUNO;
                } else if (strcmp(possible_type, "professor") == 0) {
                    user->type = PROFESSOR;
                } else if (strcmp(possible_type, "administrator") == 0) {
                    user->type = ADMINISTRADOR;
                } else {
                    // incorrect format
                    sem_post(config_sem);
                    return -1;
                }
                sem_post(config_sem);
                return 0;
            } else {
                / password mismatch /
                sem_post(config_sem);
                return 2;
            }
        }
        id++;
    }
    sem_post(config_sem);
    */
    return 1;
}

// Commands for TCP client
int list_cmds_tcp(char *response) {
    /* Lists all commands for TCP client */
    sprintf(response + strlen(response), "List of TCP commands:\n"
                                         "  - LOGIN <user_name> <password>\n"
                                         "  - HELP\n"
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
    /* Lists all classes */
    response[0] = '\0'; // clear response buffer
    sem_wait(class_sem);
    sprintf(response + strlen(response), "List of classes:\n");
    for (int i = 0; i < n_classes; i++) {
        if (classes[i].name[0] == '\0') {
            // no class on this slot
            continue;
        }
        sprintf(response + strlen(response), "\t- \"%s\": vacant-%d\n", classes[i].name, classes[i].size-classes[i].subscribed);
    }
    sem_post(class_sem);
    return 0;
}

int list_subscribe(struct User *user, char *response) {
    /* Lists all subscribers of "user" */
    response[0] = '\0'; // clear response buffer
    sprintf(response+strlen(response), "List of classes subscribed by user \"%s\":\n", user->name);
    for (int i=0; i<n_classes; i++) {
        if (classes[i].name[0] == '\0') {
            continue;
        }
        for (int j=0; j<classes[i].subscribed; j++) {
            if (classes[i].subscribed_ids[j] == user->user_id) {
                sprintf(response+strlen(response), "\t-> %s\n", classes[i].name);
                break;
            }
        }
    }
    return 0;
}

int subscribe_class(struct User *user, char *class_name, char *response) {
    /* Subscribe user to class
     *  return:
     *      -1-> class not found / unknown error
     *       0-> subscription successful
     *       1-> subscription failed (class full / already subscribed)
     */
    int result;
    sem_wait(class_sem);
    for (int i=0; i<n_classes; i++) {
        if ( classes[i].name[0] == '\0') {
            // no class on this slot
            continue;
        } else if (strcmp(classes[i].name, class_name)==0) {
            // class found
            result = addsub_classstruct(&classes[i], user->user_id);
            if ( result==0) {
                // subscription successful
                sem_post(class_sem);
                sprintf(response+strlen(response), "User \"%s\" subscribed to class \"%s\".\n", user->name, class_name);
                return 0;
            } else if ( result==-1 ) {
                // class full
                sem_post(class_sem);
                sprintf(response+strlen(response), "!!!ERROR!!!\n-> Class \"%s\" is full.\n", class_name);
                return 1;
            } else if ( result==1 ) {
                // already subscribed
                sem_post(class_sem);
                sprintf(response+strlen(response), "!!!ERROR!!!\n-> User \"%s\" is already subscribed to class \"%s\".\n", user->name, class_name);
                return 1;
            } else {
                // unknown error
                sem_post(class_sem);
                sprintf(response+strlen(response), "!!!ERROR!!!\n-> Unknown error while subscribing user \"%s\" to class \"%s\".\n", user->name, class_name);
                return -1;
            
            }
        } else {
            // class not found
            printf("\"%s\" not \"%s\".\n", classes[i].name, class_name);
        }
    }
    // class not found
    sem_post(class_sem);
    sprintf(response+strlen(response), "!!!ERROR!!!\n-> Class \"%s\" not found.\n", class_name);
    return -1;
}

int create_class(struct User *user, char *class_name, int size, char *response) {
    /* Creates class on array classes
     *
     * return:
     *      ERRORS:
     *          -1-> malloc failed
     *          -2-> class already exists
     *          -3-> no more space for classes
     *     SUCESS:
     *          (any positive int)-> index of new class
     */
    int new_index = -1;
    response[0] = '\0'; // clear response buffer
    sem_wait(class_sem);
    for (int i = 0; i < n_classes; i++) {
        if (classes[i].name[0] == '\0') {
            // empty spot found
            if (new_index == -1) {
                // first empty spot found
                new_index = i;
            }
            continue;
        }
        if (strcmp(classes[i].name, class_name) == 0) {
            // class already exists
            sem_post(class_sem);
            sprintf(response + strlen(response), "!!!ERROR!!!\n-> Class \"%s\" already exists!\n", class_name);
            return -2;
        }
    }
    if (new_index == -1) {
        // no more space for classes
        sem_post(class_sem);
        sprintf(response + strlen(response), "!!!ERROR!!!\n-> No more space for classes!\n");
        return -3;
    } else {
        create_classstruct(&classes[new_index], class_name, size);
        if (classes[new_index].name[0] == '\0') {
            // creation failed
            sem_post(class_sem);
            sprintf(response + strlen(response), "!!!ERROR!!!\n-> Could not create class \"%s\".\n", class_name);
            return -1;
        }
        // creation successful, class created
        sem_post(class_sem);
        sprintf(response + strlen(response), "Class \"%s\" created with size %d on slot %d.\n", classes[new_index].name, classes[new_index].size, new_index);
        return new_index;
    }
}

int send_message(struct User *user, char *class_name, char *message) {
    // TODO
    printf("[TODO]: User \"%s\" send message to class \"%s\", message \"%s\".\n", user->name, class_name, message);
    return -1;
}


// Commands for UDP client
int list_cmds_udp(char *response) {
    /* Lists all commands for UDP client */
    sprintf(response + strlen(response), "List of UDP commands:\n"
                                         "  - LOGIN <user_name> <password>\n"
                                         "  - HELP\n"
                                         "  -> After login:\n"
                                         "      - ADD_USER <username> <password> <type>\n"
                                         "      - DEL <username>\n"
                                         "      - LIST\n"
                                         "      - QUIT_SERVER\n");
    return 0;
}

int add_user(struct User *user, char *user_name, char *password, char *type, char *response) {
    /* Adds user to config file with "user_name" "password" and "type", if type is valid
     *      return:
     *         -1-> invalid type
     *         (positive integer)-> user added
     */
    int ret;
    ret = file_adduser(config_file_path, user_name, password, type);

    if (ret == 0) {
        sprintf(response, "User \"%s\" with password \"%s\" and type <%s> added to config file.\n", user_name, password, type);
        return 0;
    } else if (ret == 1) {
        sprintf(response, "User \"%s\" already exists in config file.\n", user_name);
        return 1;
    } else if (ret == -1) {
        sprintf(response, "!!!ERROR!!!\n-> Could not add user \"%s\" to config file [File may be corrupted].\n", user_name);
        return -1;
    } else if (ret == -2) {
        sprintf(response, "!!!ERROR!!!\n-> Invalid type \"%s\" (must be \"aluno\" \"professor\" \"administrador\").\n", type);
        return -2;
    }



    return 0;
}

int del_user(struct User *user, char *user_name, char* response) {
    /* Deletes entry on config file based on "user_name" */
    int ret;
    ret = file_removeuser(config_file_path, user_name);
    if (ret == 0) {
        sprintf(response, "User \"%s\" deleted from config file.\n", user_name);
        return 0;
    } else if (ret == 1) {
        sprintf(response, "User \"%s\" not found in config file.\n", user_name);
        return 1;
    } else {
        sprintf(response, "!!!ERROR!!!\n-> Could not delete user \"%s\" from config file [File may be corrupted].\n", user_name);
        return -1;
    }
    
    return -1;
}

int list_users(struct User *user, char *response) {
    /* Lists all users */

    file_listusers(config_file_path, response);

    return 0;
}
