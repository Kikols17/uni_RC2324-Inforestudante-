#ifndef FILE_MANAGER_C
#define FILE_MANAGER_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include "file_manager.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

extern sem_t *config_sem;


int file_checkintegrity(char *filename) {
    /* Check integrity of file
     * Inputs:
     *      filename: name of file 
     *
     * Returns:
     *       0: file is correct
     *      -1: file not found
     *      -2: invalid format
     */
    sem_wait(config_sem);
    FILE *file;

    file = fopen(filename, "r");
    if (file == NULL) {
        // file not found
        sem_post(config_sem);
        return -1;
    }

    char *line = NULL;
    size_t len = BUF_SIZE;
    size_t size;

    char *username, *password, *type;
    while ( (int)(size = getline(&line, &len, file))!=-1 ) {
        // analyze line
        line[size-1] = '\0'; // remove '\n' from end of line
        username = strtok(line, ";");
        password = strtok(NULL, ";");
        type = strtok(NULL, "\n");
        //printf("Size: %ld, Arguments: \"%s\" \"%s\" \"%s\"\n", size, username, password, type);
        if (username==NULL || password==NULL || type==NULL) {
            // incorrect amount of arguments
            fclose(file);
            sem_post(config_sem);
            return -2;
        }
        if (strcmp(type, "aluno") && strcmp(type, "professor") && strcmp(type, "administrator")) {
            // incorrect type
            printf("Incorrect type: %s\n", type);
            fclose(file);
            sem_post(config_sem);
            return -2;
        }
    }
    // Reached end of file, must be correct
    fclose(file);
    sem_post(config_sem);
    return 0;
}

int file_finduser(char *filename, char *username, char *password, char *type) {
    /* Find user in file
     * Inputs:
     *      filename: name of file
     *      username: username to find
     *      password: password to check
     *      type: empty buffer to write the type, if successful
     * 
     * Returns:
     *       2: user found, password mismatch
     *       1: user not found
     *       0: user found, password match
     *      -1: file not found / corrupted
     */
    sem_wait(config_sem);
    FILE *file;

    file = fopen(filename, "r");
    if (file == NULL) {
        // file not found
        sem_post(config_sem);
        return -1;
    }

    char *line = NULL;
    size_t len = BUF_SIZE;
    size_t size;

    char *possible_username, *possible_password, *possible_type;
    while ( (int)(size = getline(&line, &len, file))!=-1 ) {
        possible_username = strtok(line, ";");
        possible_password = strtok(NULL, ";");
        possible_type = strtok(NULL, "\n");
        if (possible_username==NULL || possible_password==NULL || possible_type==NULL) {
            // incorrect amount of arguments
            fclose(file);
            sem_post(config_sem);
            return -1;
        }
        if (strcmp(possible_username, username)==0) {
            // user found
            if (strcmp(possible_password, password)==0) {
                // password match
                strcpy(type, possible_type);
                fclose(file);
                sem_post(config_sem);
                return 0;
            }
            else {
                // password mismatch
                fclose(file);
                sem_post(config_sem);
                return 2;
            }
        }
    }

    fclose(file);
    sem_post(config_sem);
    return 1;
}

int file_adduser(char *filename, char *username, char *password, char *type) {
    /* Add user to file
     * Inputs:
     *      filename: name of file
     *      username: username to add
     *      password: password to add
     *      type: type to add
     * 
     * Returns:
     *       1: user already exists
     *       0: user added
     *      -1: error opening file / corrupted
     *      -2: incorrect type
     */
    sem_wait(config_sem);
    FILE *file;

    file = fopen(filename, "r+");
    if (file == NULL) {
        // error opening file
        sem_post(config_sem);
        return -1;
    }

    // check if "type" is valid
    if (strcmp(type, "aluno")!=0 && strcmp(type, "professor")!=0 && strcmp(type, "administrator")!=0) {
        // incorrect type
        fclose(file);
        sem_post(config_sem);
        return -2;
    }

    char *line = NULL;
    size_t len = BUF_SIZE;
    size_t size;

    char *possible_username, *possible_password, *possible_type;
    while ( (int)(size = getline(&line, &len, file))!=-1 ) {
        possible_username = strtok(line, ";");
        possible_password = strtok(NULL, ";");
        possible_type = strtok(NULL, "\n");
        if (possible_username==NULL || possible_password==NULL || possible_type==NULL) {
            // incorrect amount of arguments
            fclose(file);
            sem_post(config_sem);
            return -1;
        }
        if (strcmp(possible_username, username)==0) {
            // user already exists
            fclose(file);
            sem_post(config_sem);
            return 1;
        }
    }

    fprintf(file, "%s;%s;%s\n", username, password, type);
    fclose(file);
    sem_post(config_sem);
    return 0;
}

int file_removeuser(char *filename, char *username) {
    /* Remove user from file
     * Inputs:
     *      filename: name of file
     *      username: username to remove
     * 
     * Returns:
     *       1: user not found
     *       0: user removed
     *      -1: error opening file / corrupted
     */
    sem_wait(config_sem);
    FILE *file;

    file = fopen(filename, "r");
    if (file == NULL) {
        // error opening file
        sem_post(config_sem);
        return -1;
    }

    char writing_buffer[BUF_SIZE] = "";
    int ret = 1;    // 1->user not found; 0->user found

    char *line = NULL;
    char line_aux[BUF_SIZE];
    size_t len = BUF_SIZE;
    size_t size;

    char *possible_username, *possible_password, *possible_type;
    while ( (int)(size = getline(&line, &len, file))!=-1 ) {
        strcpy(line_aux, line);
        possible_username = strtok(line, ";");
        possible_password = strtok(NULL, ";");
        possible_type = strtok(NULL, "\n");
        if (possible_username==NULL || possible_password==NULL || possible_type==NULL) {
            // incorrect amount of arguments
            fclose(file);
            sem_post(config_sem);
            return -1;
        }
        if (strcmp(possible_username, username)!=0) {
            // not user, copy to writing buffer
            strcat(writing_buffer, line_aux);
        } else {
            // user found
            ret = 0;
        }
    }
    fclose(file);

    file = fopen(filename, "w");
    if (file == NULL) {
        // error opening file
        sem_post(config_sem);
        return -1;
    }
    fprintf(file, "%s", writing_buffer);    // overwrite file with writing buffer (user removed)
    fclose(file);
    sem_post(config_sem);
    return ret;
}

int file_listusers(char *filename, char *response) {
    /* List users in file
     * Inputs:
     *      filename: name of file
     *      response: buffer to write response
     * 
     * Returns:
     *      0: success
     *     -1: error opening file / corrupted
     */
    sem_wait(config_sem);
    FILE *file;
    
    file = fopen(filename, "r");
    if (file == NULL) {
        // error opening file
        sem_post(config_sem);
        return -1;
    }

    sprintf(response, "Users:\n");

    char *line = NULL;
    size_t len = BUF_SIZE;
    size_t size;
    
    char *possible_username, *possible_password, *possible_type;
    while ( (int)(size = getline(&line, &len, file))!=-1 ) {
        possible_username = strtok(line, ";");
        possible_password = strtok(NULL, ";");
        possible_type = strtok(NULL, "\n");
        if (possible_username==NULL || possible_password==NULL || possible_type==NULL) {
            // incorrect amount of arguments
            fclose(file);
            sem_post(config_sem);
            return -1;
        }
        sprintf(response+strlen(response), "\t%s -> ", possible_username);
        if ( strcmp(possible_type, "aluno")==0 ) {
            sprintf(response+strlen(response), "\033[1;34m");
        } else if ( strcmp(possible_type, "professor")==0 ) {
            sprintf(response+strlen(response), "\033[1;32m");
        } else {
            sprintf(response+strlen(response), "\033[1;31m");
        }
        sprintf(response+strlen(response), "%s", possible_type);
        sprintf(response+strlen(response), "\033[0m\n");
    }
    fclose(file);
    sem_post(config_sem);
    return 0;
}

#endif