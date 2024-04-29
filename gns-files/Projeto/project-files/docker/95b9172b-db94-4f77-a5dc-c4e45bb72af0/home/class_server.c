#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#include "commands_server.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif


void handle_sigint();
void *handle_tcp(void *PORTO_TURMAS);
void *handle_udp(void *PORTO_CONFIG);

void process_client_tcp(int client_fd_tcp);
void process_admin_udp();
int handle_requests_tcp(struct User *user, char *request, char *response);
int handle_requests_udp(struct User *user, char *request, char *response);



FILE *config_file;
pid_t pid;
int server_fd_tcp = -1, server_fd_udp = -1;
int client_fd_tcp = -1;


int main(int argc, char *argv[]) {
    pthread_t udp_thread, tcp_thread;

    int PORTO_TURMAS;
    int PORTO_CONFIG;


    signal(SIGINT, handle_sigint);      // used to close server correctly

    if (argc!=4) {
        // make sure start arguments are correct
        printf("!!!INVALID ARGUMENTS!!!\n-> server <PORTO_TURMAS> <PORTO_CONFIG> <CONFIG_FILEPATH>\n");
        return 1;
    }

    PORTO_TURMAS = atoi(argv[1]);
    PORTO_CONFIG = atoi(argv[2]);
    if ( !(1024<=PORTO_TURMAS  &&  PORTO_TURMAS<=65535)    ||    !(1024<=PORTO_CONFIG  &&  PORTO_CONFIG<=65535) ) {
        // make sure ports are valid
        printf("!!!INVALID ARGUMENT!!!\n-> <PORTO_TURMAS> & <PORTO_CONFIG> must be integers between 1024-65535\n");
        return 1;
    }

    if (PORTO_TURMAS==PORTO_CONFIG) {
        // make sure ports are not the same
        printf("!!!INVALID ARGUMENT!!!\n-> <PORTO_TURMAS> & <PORTO_CONFIG> cannot be the same\n");
        return 1;
    }

    config_file = fopen(argv[3], "r");
    if (config_file==NULL) {
        // make sure the config file exists
        printf("!!!INVALID ARGUMENTS!!!\n-> <CONFIG_FILEPATH> not found\n");
        return 1;
    }


    pthread_create(&tcp_thread, NULL, handle_tcp, &PORTO_TURMAS);        // handle tcp connections
    pthread_create(&udp_thread, NULL, handle_udp, &PORTO_CONFIG);        // handle udp connections
    
    pthread_join(udp_thread, NULL);
    pthread_join(tcp_thread, NULL);

    return 0;
}





void handle_sigint() {
    if (pid!=0) {
        waitpid(-1, NULL, 0);       // wait for clients to be disconnected before we can close the server_fd_tcp
        printf("-> Closing server_fd_tcp: %d\n", server_fd_tcp);
        close(server_fd_tcp);

        printf("-> Closing server_fd_udp: %d\n", server_fd_udp);
        close(server_fd_udp);

        printf("-> Closing Server\n");
    }
    if (pid==0) {
        write(client_fd_tcp, "-+!SERVER-CL0SING!+-", 1 + strlen("-+!SERVER-CL0SING!+-"));
        printf("-> Closing client_fd_tcp: %d\n", client_fd_tcp);
        close(client_fd_tcp);
    }
    fclose(config_file);
    exit(1);
}


void *handle_tcp(void *PORTO_TURMAS_ptr) {
    /* handle tcp connections (user connections) */
    int PORTO_TURMAS = *((int*) PORTO_TURMAS_ptr);      // get PORTO_CONFIG
    
    struct sockaddr_in addr, client_addr;
    int client_addr_size;

    bzero((void *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORTO_TURMAS);

    if ((server_fd_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("!!!ERROR!!!\n-> Could not create socket.\n");
        exit(1);
    }
    if (bind(server_fd_tcp, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("!!!ERROR!!!\n-> Could not bind to addr.\n");
        exit(1);
    }
    if (listen(server_fd_tcp, 5) < 0) {
        printf("!!!ERROR!!!\n-> Could not listen to socket\n.");
        exit(1);
    }
    client_addr_size = sizeof(client_addr);
    while (1) {
        // clean finished child processes, avoiding zombies
        // must use WNOHANG or would block whenever a child process was working
        while (waitpid(-1, NULL, WNOHANG) > 0);

        // wait for new connection
        client_fd_tcp = accept(server_fd_tcp, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
        if (client_fd_tcp > 0) {
            pid = fork();
            if (pid == 0) {
                // if not parent process
                close(server_fd_tcp);
                process_client_tcp(client_fd_tcp);
                exit(0);
            }
            close(client_fd_tcp);
        }
    }
}

void *handle_udp(void *PORTO_CONFIG_ptr) {
    /* handle udp connections (config connections) */
    int PORTO_CONFIG = *((int*) PORTO_CONFIG_ptr);      // get PORTO_CONFIG

    struct sockaddr_in si_minha;            // Structs for the addresses

    // Cria um socket para recepção de pacotes UDP
    if((server_fd_udp=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("!!!ERROR!!!\n-> Could not create socket.\n");
        exit(1);
    }

    // Preenchimento da socket address structure
    si_minha.sin_family = AF_INET;		// set IPv4
    si_minha.sin_port = htons(PORTO_CONFIG);	// set port
    si_minha.sin_addr.s_addr = INADDR_ANY;

    // Associa o socket à informação de endereço
    if(bind(server_fd_udp,(struct sockaddr*)&si_minha, sizeof(si_minha)) == -1) {
        printf("!!!ERROR!!!\n-> Could not bind to addr.\n");
        exit(1);
    }

    process_admin_udp(server_fd_udp);

    close(server_fd_udp);
    return NULL;
}




void process_client_tcp(int client_fd_tcp) {
    int nread = 0;
    char buffer_in[BUF_SIZE];       // buffer para guardar msgs de entrada
    char buffer_out[BUF_SIZE];      // buffer para escrever as msgs de saida
    char welcome_message[] = "Welcome to Server. Login with:\nLOGIN <user_name> <password>";  // message to be sent in the beggining

    struct User user;           // }
    user.user_id = -1;          // } used to autenticate user


    write(client_fd_tcp, welcome_message, 1 + strlen(welcome_message));     // } welcome new client
    printf("[TCP]<<<<< WELCOME fd->%d.\n", client_fd_tcp);                  // }

    while (1) {
        nread = read(client_fd_tcp, buffer_in, BUF_SIZE - 1);                   // }
        buffer_in[nread] = '\0';                                                // } read request from client
        printf("[TCP]>>>>> FROM fd->%d: \"%s\"\n", client_fd_tcp, buffer_in);   // }

        if (  strcmp(buffer_in, "-+!QUIT!+-")==0  ) {                   // }
            printf("[TCP]----- DISCONNECTING fd %d.\n", client_fd_tcp); // } if client disconnects, close it's fd
            break;                                                      // }
        }

        handle_requests_tcp(&user, buffer_in, buffer_out);

        write(client_fd_tcp, buffer_out, 1 + strlen(buffer_out));               // }
        printf("[TCP]<<<<< TO fd->%d: \"%s\".\n", client_fd_tcp, buffer_out);   // } send response back to the client
        

        fflush(stdout);
    }
    close(client_fd_tcp);
}

void process_admin_udp() {
    struct sockaddr_in si_outra;
    socklen_t slen = sizeof(si_outra);

    int recv_len;		// tamanho da transmissão recebida
    char buf_in[BUF_SIZE];	    // transmissão recebida
    char buf_out[BUF_SIZE] = "";	// transmissão a ser enviada

    struct User user;           // }
    user.user_id = -1;          // } used to autenticate user (admin)

    int running = 1;
    while (running==1) {
        slen = sizeof(si_outra);     //slen is value/result 

        if((recv_len = recvfrom(server_fd_udp, buf_in, BUF_SIZE, 0, (struct sockaddr *) &si_outra, (socklen_t *) &slen)) == -1) {
            printf("!!!ERROR!!!\n-> Error in recvfrom.\n");
            exit(1);
        }
	    buf_in[recv_len-1] = ' ';
	    buf_in[recv_len-1] = '\0';
        printf("[UDP]>>>>> FROM client port->%d: \"%s\".\n", si_outra.sin_port, buf_in);

        if ( handle_requests_udp(&user, buf_in, buf_out)==-1 ) {
            running = 0;
        }

        printf("[UDP]<<<<< TO client port->%d: \"%s\".\n", si_outra.sin_port, buf_out);
        sendto(server_fd_udp, (const char *)buf_out, strlen(buf_out), MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen); 
    }
    handle_sigint();
}




int handle_requests_tcp(struct User *user, char *request, char *response) {
    /* Interpret and handle user requests */
    char *command = strtok(request, " ");     // get first argument
    char *arg1, *arg2, *end;
    //printf("command: \"%s\"\n", command);

    response[0] = '\0';     // clear response buffer


    if ( strcmp(command, "LOGIN")==0 ) {
        /* Log in user (LOGIN <username> <password>) */
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || arg2==NULL || end!=NULL) {
            sprintf(response, "-> INVALID ARGUMENTS:\nLOGIN <username> <password>");
            return 1;
        } else {
            if (login(user, arg1, arg2)==0 ) {
                // login sucessful
                if ( user->type==ADMINISTRADOR ) {
                    // user is "administrador", reject
                    sprintf(response, "User \"%s\" with password \"%s\" is admin, cannot log in.\nUse UDP connection instead.", arg1, arg2);
                    user->user_id=-1;           // }
                    strcpy(user->name, "");     // } reset user;
                    return 2;
                } else {
                    // user is "aluno" ou "professor"
                    char type[BUF_SIZE];
                    if ( user->type==ALUNO ) { strcpy(type, "aluno"); }     // } see if is aluno
                    else { strcpy(type, "professor"); }                     // } / professor
                    sprintf(response, "Now logged in as %s \"%s\" with password \"%s\"", type, arg1, arg2);
                }
            } else {
                // login unsucessful
                sprintf(response, "Could not log in user \"%s\" with password \"%s\"", arg1, arg2);
                return 2;
            }
            return 0;
        }
    

    } else if (user->user_id==-1) {
        /* If not logged in, request to log in */
        sprintf(response, "-> INVALID CREDENTIALS:\ncannot perform any actions, login first\nLOGIN <username> <password>");
        return 2;


    } else if ( strcmp(command, "LIST_CLASSES")==0 ) {
        /* List classes for this user (LIST_CLASSES) */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response, "-> INVALID ARGUMENTS:\nLIST_CLASSES");
            return 1;
        } else {
            // TODO[META1]  list all classes
            sprintf(response, "Now listing classes for user ID:%d, name:\"%s\".", user->user_id, user->name);
            return 0;
        }


    } else if ( strcmp(command, "LIST_SUBSCRIBED")==0 ) {
        /* List subscriptions for this user (LIST_SUBSCRIPTED) */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response, "-> INVALID ARGUMENTS:\nLIST_SUBSCRIBED");
            return 1;
        } else {
            // TODO[META1]  list class subscriptions for user
            sprintf(response, "Now listing subscriptions for user ID:\"%d\", name:\"%s\".", user->user_id, user->name);
            return 0;
        }


    } else if ( strcmp(command, "SUBSCRIBE_CLASS")==0 ) {
        /* Subscribe this user to new class (SUBSCRIBE_CLASS <class_name>) */
        arg1 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || end!=NULL) {
            sprintf(response, "-> INVALID ARGUMENTS:\nSUBSCRIBE_CLASS <class_name>");
            return 1;
        } else {
            // TODO[META1] subscribe user to class
            sprintf(response, "Now subscribing user ID:\"%d\", name:\"%s\" to class named \"%s\".", user->user_id, user->name, arg1);
            return 0;
        }
        

    } else if ( strcmp(command, "CREATE_CLASS")==0 ) {
        /* Create new class by this user(professor) */
        if (user->type==PROFESSOR) {
            // If user is a "professor"
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            end = strtok(NULL, " ");
            if (arg1==NULL || arg2==NULL || end!=NULL) {
                sprintf(response, "-> INVALID ARGUMENTS:\nCREATE_CLASS <class_name> <size>");
                return 1;
            } else {
                // TODO[META1] create class with name and size
                sprintf(response, "Now creating class \"%s\" with size \"%s\".", arg1, arg2);
                return 0;
            }
        } else {
            // If user is not a "professor"
            sprintf(response, "-> INVALID CREDENTIALS:\nuser ID:%d, name:\"%s\" must be \"professor\".", user->user_id, user->name);
            return 2;
        }
    

    } else if ( strcmp(command, "SEND")==0 ) {
        /* Send message to a class(professor) (SEND <class_name> <text that server will send to subscribers>) */
        if (user->type==PROFESSOR) {
            // If user is a "professor"
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            // no size limit, no *end pointer
            if (arg1==NULL || arg2==NULL) {
                sprintf(response, "-> INVALID ARGUMENTS:\nSEND <class_name> <text that server will send to subscribers>");
                return 1;
            } else {
                // TODO[META1] create class with name and size
                sprintf(response, "Now sending message \"%s\" to class \"%s\".", arg2, arg1);
                return 0;
            }
        } else {
            // If user is not a "professor"
            sprintf(response, "-> INVALID CREDENTIALS:\nuser ID:%d, name:\"%s\" must be \"professor\".", user->user_id, user->name);
            return 2;
        }
    

    } else {
        /* Command not found */
        sprintf(response, "-> INVALID COMMAND!");
        return -1;
    }
    

    return -1;
}

int handle_requests_udp(struct User *user, char *request, char *response) {
    /* interpret and handle user requests */
    char *command = strtok(request, " ");     // get first argument
    char *arg1, *arg2, *arg3, *end;

    response[0] = '\0';     // clear response buffer



    if ( strcmp(command, "LOGIN")==0 ) {
        /* Log in user(administrador) (LOGIN <username> <password>) */
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || arg2==NULL || end!=NULL) {
            sprintf(response, "-> INVALID ARGUMENTS:\nLOGIN <username> <password>\n\n");
            return 1;
        } else {
            if ( login(user, arg1, arg2)==0 ) {
                // Login successful
                if ( user->type == ADMINISTRADOR) {
                    // if user is admin, continue
                    sprintf(response, "Now logged in as admin \"%s\" with password \"%s\".\n\n", arg1, arg2);
                    return 0;
                } else {
                    // if user is not admin, revoke login
                    sprintf(response, "-> INVALID CREDENTIALS:\nto access these features \"%s\" must be \"administrador\".\nTry TCP connection for \"aluno\" or \"professor\".\n\n", arg1);
                    user->user_id = -1;     // }
                    strcpy(user->name, ""); // } reset user
                    return 2;
                }
            } else {
                // If login fails"
                sprintf(response, "Could not log in user \"%s\" with password \"%s\"", arg1, arg2);
                return 2;
            }
        }


    } else if (user->user_id==-1 || user->type!=ADMINISTRADOR) {
        /* Handle permissions (only "administradores" can run commands further down) */
        // if this user has no session initiated
        sprintf(response, "-> PERMISSION DENIED:\nmust login as admin to run commands (see LOGIN).\n\n");
        return 2;
    

    } else if ( strcmp(command, "ADD_USER")==0 ) {
        /* Add new user (ADD_USER <username> <password> <type>) */
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
        arg3 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || arg2==NULL || arg3==NULL || end!=NULL) {
            sprintf(response, "-> INVALID ARGUMENTS:\nLOGIN <username> <password> <type>\n\n");
            return 1;
        } else {
            // TODO[META1] create user
            sprintf(response, "Creating new user \"%s\" with password \"%s\" and type \"%s\".\n\n", arg1, arg2, arg3);
            add_user(user, arg1, arg2, arg3);
            return 0;
        }
    
    } else if ( strcmp(command, "DEL")==0 ) {
        /* Delete user (DEL <user>) */
        arg1 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || end!=NULL) {
            sprintf(response, "-> INVALID ARGUMENTS:\nDEL <username>\n\n");
            return 1;
        } else {
            // TODO[META1] delete user
            sprintf(response, "Deleting user \"%s\".\n\n", arg1);
            del_user(user, arg1);
            return 0;
        }
    

    } else if ( strcmp(command, "LIST")==0 ) {
        /* List all users (LIST) */
        // TODO[META1] list users
        sprintf(response, "Listing users.\n\n");
        list_users(user);
        return 0;
    

    } else if ( strcmp(command, "QUIT_SERVER")==0 ) {
        /* Close Server */
        sprintf(response, "SERVER CLOSING.\n\n");
        return -1;


    } else {
        // Command not found
        sprintf(response, "-> INVALID COMMAND!: \"%s\"\n\n", command);
        return 1;
    }

    return 1;
}