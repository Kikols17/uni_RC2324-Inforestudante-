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
#include <semaphore.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>


#include "commands_server.h"
#include "class_struct.h"
#include "file_manager.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif
#define N_CLASSES 16
#define N_USERS 124


void system_shutdown();
void close_main();
void close_tcp();

int create_shared_memory();

void *handle_tcp(void *PORTO_TURMAS);
void *handle_udp(void *PORTO_CONFIG);

void process_client_tcp(int client_fd_tcp);
void process_admin_udp();
int handle_requests_tcp(struct User *user, char *request, char *response);
int handle_requests_udp(struct User *user, char *request, char *response);
void handle_usecursor(struct User *user, int admin_flag, char *response);



pid_t pid;
pid_t main_pid;

int server_fd_tcp = -1, server_fd_udp = -1;
int client_fd_tcp = -1;
int n_classes = N_CLASSES;

// create shared memory
int shmid;
Class *classes;
sem_t *class_sem;

// child stuff
int shmid_children;
int *child_pids;
sem_t *child_sem;


// file stuff
char config_file_path[BUF_SIZE];
sem_t *config_sem;


int main(int argc, char *argv[]) {
    pthread_t udp_thread, tcp_thread;

    int PORTO_TURMAS;
    int PORTO_CONFIG;

    int ret;


    signal(SIGINT, close_main);             // used to close server correctly
    signal(SIGQUIT, close_main);            // used to close server correctly
    main_pid = getpid();                // get main process id

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

    // create shared memory
    if (create_shared_memory()==-1) {
        // could not create shared memory
        printf("!!!ERROR!!!\n-> Could not create shared memory.\n");
        return 1;
    }

    // create semaphore
    class_sem = sem_open("class_sem", O_CREAT, 0666, 1);
    if (class_sem == SEM_FAILED) {
        // could not create semaphore
        printf("!!!ERROR!!!\n-> Could not create semaphore.\n");
        return 1;
    }

    child_sem = sem_open("child_sem", O_CREAT, 0666, 1);
    if (child_sem == SEM_FAILED) {
        // could not create semaphore
        printf("!!!ERROR!!!\n-> Could not create semaphore.\n");
        return 1;
    }

    config_sem = sem_open("config_sem", O_CREAT, 0666, 1);
    if (config_sem == SEM_FAILED) {
        // could not create semaphore
        printf("!!!ERROR!!!\n-> Could not create semaphore.\n");
        return 1;
    }


    strcpy(config_file_path, argv[3]);
    // check file integrity
    ret = file_checkintegrity(config_file_path);
    if (ret==0) {
        // could not open file
        printf("File \"%s\" is OK!\n", config_file_path);
    } else if (ret==-1) {
        printf("!!!ERROR!!!\n-> Could not open file \"%s\".\n", config_file_path);
        system_shutdown();
    } else if (ret==-2) {
        printf("!!!ERROR!!!\n-> File \"%s\" is corrupted.\n", config_file_path);
        system_shutdown();
    }

    memset(child_pids, 0, N_USERS*sizeof(int));    // clear child_pids

    pthread_create(&tcp_thread, NULL, handle_tcp, &PORTO_TURMAS);        // handle tcp connections
    pthread_create(&udp_thread, NULL, handle_udp, &PORTO_CONFIG);        // handle udp connections
    
    pthread_join(udp_thread, NULL);
    pthread_join(tcp_thread, NULL);

    return 0;
}




void system_shutdown() {
    kill(main_pid, SIGQUIT);
}

void close_main() {
    printf("\n\n!!!SERVER CLOSING!!!\n");
    for (int i=0; i<N_USERS; i++) {
        if (child_pids[i]!=0) {
            kill(child_pids[i], SIGQUIT);
            waitpid(child_pids[i], NULL, 0);
        }
    }

    printf("-> Closing server_fd_tcp: %d\n", server_fd_tcp);
    close(server_fd_tcp);

    printf("-> Closing server_fd_udp: %d\n", server_fd_udp);
    close(server_fd_udp);

    printf("-> Closing Server\n");

    sem_close(class_sem);       // }
    sem_unlink("class_sem");    // } close semaphore
    sem_close(config_sem);          // }
    sem_unlink("config_sem");       // } close semaphore
    sem_close(child_sem);               // }
    sem_unlink("child_sem");            // } close semaphore
    shmdt(classes);                         // }
    shmctl(shmid, IPC_RMID, NULL);          // } close shared memory
    shmdt(child_pids);                          // }
    shmctl(shmid_children, IPC_RMID, NULL);     // } close shared memory

    exit(1);
}

void close_tcp() {
    write(client_fd_tcp, "-+!SERVER-CL0SING!+-", 1 + strlen("-+!SERVER-CL0SING!+-"));
    printf("-> Closing client_fd_tcp: %d\n", client_fd_tcp);
    close(client_fd_tcp);

    sem_wait(child_sem);
    for (int i=1; i<N_USERS+1; i++) {
        if (child_pids[i]==getpid()) {
            child_pids[i] = 0;
            break;
        }
    }
    child_pids[0]--;
    sem_post(child_sem);

    exit(1);
}

int create_shared_memory() {
    shmid = shmget(IPC_PRIVATE, sizeof(Class)*N_CLASSES, IPC_CREAT | 0666);
    if (shmid < 0) {
        printf("!!!ERROR!!!\n-> Could not create shared memory.\n");
        kill(main_pid, SIGQUIT);
    }
    classes = (Class *)shmat(shmid, NULL, 0);
    if (classes == (Class *)-1) {
        printf("!!!ERROR!!!\n-> Could not assign shared memory.\n");
        kill(main_pid, SIGQUIT);
    }
    // set all classes to empty
    for (int i=0; i<N_CLASSES; i++) {
        classes[i].name[0] = '\0';
        classes[i].size = -1;
        classes[i].subscribed = 0;
        for (int j=0; j<classes[i].size; j++) {
            classes[i].subscribed_names[j][0] = '\0';
        }
        //printf("Class %d: \"%s\", size:%d, subscribed:%d\n", i, classes[i].name, classes[i].size, classes[i].subscribed);
    }

    shmid_children = shmget(IPC_PRIVATE, sizeof(int)*(1+N_USERS), IPC_CREAT | 0666);
    if (shmid_children < 0) {
        printf("!!!ERROR!!!\n-> Could not create shared memory.\n");
        kill(main_pid, SIGQUIT);
    }
    child_pids = (int *)shmat(shmid_children, NULL, 0);
    if (child_pids == (int *)-1) {
        printf("!!!ERROR!!!\n-> Could not assign shared memory.\n");
        kill(main_pid, SIGQUIT);
    }
    bzero(child_pids, (N_USERS+1)*sizeof(int));
    return 0;
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
        sem_wait(child_sem);
        if (client_fd_tcp > 0  &&  child_pids[0]<N_USERS) {
            sem_post(child_sem);
            pid = fork();
            if (pid == 0) {
                // if not parent process
                close(server_fd_tcp);
                signal(SIGINT, SIG_IGN);
                signal(SIGQUIT, close_tcp);
                fprintf (stdout,"[TCP]+++++NEW CONNECTION FROM %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                process_client_tcp(client_fd_tcp);
                kill(getpid(), SIGQUIT);
            }

            sem_wait(child_sem);
            for (int i=1; i<N_USERS+1; i++) {
                if (child_pids[i]==0) {
                    child_pids[i] = pid;
                    break;
                }
            }
            child_pids[0]++;
            sem_post(child_sem);
            close(client_fd_tcp);
        } else {
            sem_post(child_sem);
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
    char *msg;
    char welcome_message[] = "Welcome to Server. Login with:\nLOGIN <user_name> <password>";  // message to be sent in the beggining

    struct User user;           // }
    user.user_id = -1;          // } used to autenticate user


    strcpy(buffer_out, welcome_message);
    handle_usecursor(&user, 0, buffer_out);
    write(client_fd_tcp, buffer_out, 1 + strlen(buffer_out));               // } welcome new client
    printf("[TCP]<<<<< WELCOME fd->%d.\n", client_fd_tcp);                  // }

    while (1) {
        nread = read(client_fd_tcp, buffer_in, BUF_SIZE - 1);                   // }
        buffer_in[nread] = '\0';                                                // } read request from client
        printf("[TCP]>>>>> FROM fd->%d: \"%s\"\n", client_fd_tcp, buffer_in);   // }

        if (  strcmp(buffer_in, "-+!QUIT!+-")==0  ) {                   // }
            printf("[TCP]----- DISCONNECTING fd %d.\n", client_fd_tcp); // } if client disconnects, close it's fd
            break;                                                      // }
        }

        buffer_out[0] = '\0';       // clear response buffer
        handle_requests_tcp(&user, buffer_in, buffer_out);
        handle_usecursor(&user, 0, buffer_out);

        msg = strtok(buffer_out, "~");
        while ( msg!=NULL ) {
            write(client_fd_tcp, msg, 1 + strlen(msg));                     // }
            printf("[TCP]<<<<< TO fd->%d: \"%s\".\n", client_fd_tcp, msg);  // } send response back to the client
            msg = strtok(NULL, "~");
            sleep(1);
        }

        fflush(stdout);
    }
    return;
}

void process_admin_udp() {
    struct sockaddr_in si_outra;
    socklen_t slen = sizeof(si_outra);

    int recv_len;		// tamanho da transmissão recebida
    char buf_in[BUF_SIZE];	    // transmissão recebida
    char buf_out[BUF_SIZE] = "";	// transmissão a ser enviada
    char welcome_message[] = "Welcome to Server. Login with:\nLOGIN <user_name> <password>";  // message to be sent in the beggining


    struct User user;           // }
    user.user_id = -1;          // } used to autenticate user (admin)

    int running = 1;
    while (running==1) {
        slen = sizeof(si_outra);     //slen is value/result 

        if((recv_len = recvfrom(server_fd_udp, buf_in, BUF_SIZE-1, 0, (struct sockaddr *) &si_outra, (socklen_t *) &slen)) == -1) {
            printf("!!!ERROR!!!\n-> Error in recvfrom.\n");
            exit(1);
        }
        printf("%d\n", recv_len);
	    buf_in[recv_len-1] = '\0';
        printf("[UDP]>>>>> FROM client port->%d: \"%s\".\n", si_outra.sin_port, buf_in);


        buf_out[0] = '\0';       // clear response buffer
        if ( strcmp(buf_in, "-+!HELLO!+-")==0 ) {
            strcpy(buf_out, welcome_message);
        } else if ( handle_requests_udp(&user, buf_in, buf_out)==-1 ) {
            running = 0;
        }
        handle_usecursor(&user, 1, buf_out);

        printf("[UDP]<<<<< TO client port->%d: \"%s\".\n", si_outra.sin_port, buf_out);
        sendto(server_fd_udp, (const char *)buf_out, BUF_SIZE-1, MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen); 
    }
    system_shutdown();
}



int handle_requests_tcp(struct User *user, char *request, char *response) {
    /* Interpret and handle user requests */
    char *command = strtok(request, " ");     // get first argument
    char *arg1, *arg2, *end;


    if ( command==NULL ) {
        /* If command is empty */
        if (user->user_id==-1) {
            // if not logged in, display with login command
            sprintf(response+strlen(response), "-> INVALID COMMAND! Login with:\nLOGIN <user_name> <password>");
        } else {
            // if logged in, display only error message
            sprintf(response+strlen(response), "-> INVALID COMMAND!\nHELP for list of commands");
        }
        return -1;

        
    } else if ( strcmp(command, "HELP")==0 ) {
        /* List all commands (HELP) */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nHELP\n");
            return 1;
        } else {
            list_cmds_tcp(response);
            return 0;
        }
    

    } else if ( strcmp(command, "LOGIN")==0 ) {
        /* Log in user (LOGIN <username> <password>) */
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || arg2==NULL || end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nLOGIN <username> <password>\n");
            return 1;
        } else {
            if (login(user, arg1, arg2, 0, response)!=0 ) {
                // login failed
                return 2;
            }
            // login successful
            return 0;
        }
    

    } else if (user->user_id==-1) {
        /* If not logged in, request to log in */
        sprintf(response+strlen(response), "-> INVALID CREDENTIALS:\ncannot perform any actions, login first\nLOGIN <username> <password>\n");
        return 2;


    } else if ( strcmp(command, "LOGOUT")==0 ) {
        /* Close user session */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nLOGOUT\n");
            return 1;
        }
        if ( logout(user, response)==0 ) {
            return 0;
        } else {
            return 2;
        }

    } else if ( strcmp(command, "LIST_CLASSES")==0 ) {
        /* List classes for this user (LIST_CLASSES) */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nLIST_CLASSES\n");
            return 1;
        } else {
            // TODO[META1]  list all classes
            //sprintf(response+strlen(response), "Now listing classes for user ID:%d, name:\"%s\".\n", user->user_id, user->name);
            list_classes(user, response);
            return 0;
        }


    } else if ( strcmp(command, "LIST_SUBSCRIBED")==0 ) {
        /* List subscriptions for this user (LIST_SUBSCRIPTED) */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nLIST_SUBSCRIBED\n");
            return 1;
        } else {
            // TODO[META1]  list class subscriptions for user
            //sprintf(response+strlen(response), "Now listing subscriptions for user ID:\"%d\", name:\"%s\".\n", user->user_id, user->name);
            list_subscribe(user, response);
            return 0;
        }


    } else if ( strcmp(command, "SUBSCRIBE_CLASS")==0 ) {
        /* Subscribe this user to new class (SUBSCRIBE_CLASS <class_name>) */
        arg1 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nSUBSCRIBE_CLASS <class_name>\n");
            return 1;
        } else {
            // TODO[META1] subscribe user to class
            subscribe_class(user, arg1, response);
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
                sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nCREATE_CLASS <class_name> <size>\n");
                return 1;
            } else {
                // TODO[META1] create class with name and size
                //sprintf(response+strlen(response), "Now creating class \"%s\" with size \"%s\".\n", arg1, arg2);
                create_class(arg1, atoi(arg2), response);

                return 0;
            }
        } else {
            // If user is not a "professor"
            sprintf(response+strlen(response), "-> INVALID CREDENTIALS:\nuser ID:%d, name:\"%s\" must be \"professor\".\n", user->user_id, user->name);
            return 2;
        }
    

    } else if ( strcmp(command, "SEND")==0 ) {
        /* Send message to a class(professor) (SEND <class_name> <text that server will send to subscribers>) */
        if (user->type==PROFESSOR) {
            // If user is a "professor"
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, "\0");
            // no size limit, no *end pointer
            if (arg1==NULL || arg2==NULL) {
                sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nSEND <class_name> <text that server will send to subscribers>\n");
                return 1;
            } else {
                // TODO[META1] create class with name and size
                //sprintf(response+strlen(response), "Now sending message \"%s\" to class \"%s\".\n", arg2, arg1);
                send_message(user, arg1, arg2, response);
                return 0;
            }
        } else {
            // If user is not a "professor"
            sprintf(response+strlen(response), "-> INVALID CREDENTIALS:\nuser ID:%d, name:\"%s\" must be \"professor\".\n", user->user_id, user->name);
            return 2;
        }
    

    } else {
        /* Command not found */
        sprintf(response+strlen(response), "-> INVALID COMMAND!\nHELP for list of commands.\n");
        return -1;
    }
    

    return -1;
}

int handle_requests_udp(struct User *user, char *request, char *response) {
    /* interpret and handle user requests */
    char *command = strtok(request, " ");     // get first argument
    char *arg1, *arg2, *arg3, *end;


    if ( command==NULL ) {
        /* If command is empty */
        sprintf(response+strlen(response), "-> INVALID COMMAND!\nHELP for list of commands\n");
        return 1;
    
    
    } else if ( strcmp(command, "HELP")==0 ) {
        /* List all commands (HELP) */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nHELP\n");
            return 1;
        } else {
            list_cmds_udp(response);
            return 0;
        }


    } else if ( strcmp(command, "LOGIN")==0 ) {
        /* Log in user(administrador) (LOGIN <username> <password>) */
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || arg2==NULL || end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nLOGIN <username> <password>\n");
            return 1;
        } else {
            if ( login(user, arg1, arg2, 1, response)!=0 ) {
                // login failed
                return 2;
            } else {
                // login successful
                return 0;
            }
        }


    } else if (user->user_id==-1 || user->type!=ADMINISTRADOR) {
        /* Handle permissions (only "administradores" can run commands further down) */
        // if this user has no session initiated
        sprintf(response+strlen(response), "-> PERMISSION DENIED:\nmust login as admin to run commands (see LOGIN).\n");
        return 2;
    

    } else if ( strcmp(command, "ADD_USER")==0 ) {
        /* Add new user (ADD_USER <username> <password> <type>) */
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
        arg3 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || arg2==NULL || arg3==NULL || end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nADD_USER <username> <password> <type>\n");
            return 1;
        } else {
            // TODO[META1] create user
            add_user(user, arg1, arg2, arg3, response);
            return 0;
        }
    
    } else if ( strcmp(command, "DEL")==0 ) {
        /* Delete user (DEL <user>) */
        arg1 = strtok(NULL, " ");
        end = strtok(NULL, " ");
        if (arg1==NULL || end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nDEL <username>\n");
            return 1;
        } else {
            // TODO[META1] delete user
            sprintf(response+strlen(response), "Deleting user \"%s\".\n", arg1);
            del_user(user, arg1, response);
            return 0;
        }
    

    } else if ( strcmp(command, "LIST")==0 ) {
        /* List all users (LIST) */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nLIST\n");
            return 1;
        } else {
            sprintf(response+strlen(response), "Listing users.\n");
            list_users(response);
        }
        return 0;
    

    } else if ( strcmp(command, "QUIT_SERVER")==0 ) {
        /* Close Server */
        end = strtok(NULL, " ");
        if (end!=NULL) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nQUIT_SERVER\n");
            return 1;
        }
        sprintf(response+strlen(response), "SERVER CLOSING.\n");
        return -1;


    } else if ( strcmp(command, "LOGOUT")==0 ) {
        /* Close admin session */
        end = strtok(NULL, " ");
        if ( end!=NULL ) {
            sprintf(response+strlen(response), "-> INVALID ARGUMENTS:\nLOGOUT\n");
            return 1;
        }
        if ( logout(user, response)==0 ) {
            return 0;
        } else {
            return 2;
        }
        return 0;
    
    } else{
        // Command not found
        sprintf(response+strlen(response), "-> INVALID COMMAND!\nHELP for list of commands\n");
        return 1;
    }

    return 1;
}

void handle_usecursor(struct User *user, int admin_flag, char *response) {
    /* Appends to "response" the name of the user, if is logged in */

    if (admin_flag==0) {
        sprintf(response+strlen(response), "^");
    } else {
        sprintf(response+strlen(response), "\n\n");
    }
    if (user->user_id==-1) {
        sprintf(response+strlen(response), "\033[1m");
    } else {
        if (user->type==ALUNO) {
            sprintf(response+strlen(response), "\033[1;34m");
        } else if (user->type==PROFESSOR) {
            sprintf(response+strlen(response), "\033[1;32m");
        } else {
            sprintf(response+strlen(response), "\033[1;31m");
        }
    }
    sprintf(response+strlen(response), "%s>\033[0m ", user->name);
}
