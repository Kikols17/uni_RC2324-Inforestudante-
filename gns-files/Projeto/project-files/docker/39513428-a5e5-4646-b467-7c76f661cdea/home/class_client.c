#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>

#define BUF_SIZE 1024
#define N_CLASSES 16
#define BASE_MULTICAST_ADDR 0xEF000001

typedef struct MT_listener {
    int state;
    pthread_t listener_thread;

    char addr[100];
    int socketfd;
    struct ip_mreq group;
}MT_listener;


void handle_sigint();
void *multicast_listener( void *arg );
int leave_multicast(int id);
void log_out();


int fd;
struct MT_listener mtlisteners[N_CLASSES];
char header[BUF_SIZE];


int main(int argc, char *argv[]) {
    int PORTO_TURMAS;
    char endServer[100];
    struct sockaddr_in addr;
    struct hostent *hostPtr;

    signal(SIGINT, handle_sigint);      // used to close client correctly

    if (argc!=3) {
        // make sure start arguments are correct
        printf("!!!INVALID ARGUMENTS!!!\n-> server <IP_ADDRESS> <PORTO_TURMAS>\n");
        exit(1);
    }

    strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0) {
        printf("!!!INVALID ARGUMENTS!!!\n-> server ip:\"%s\" not found", endServer);
        exit(1);
    }

    PORTO_TURMAS = atoi(argv[2]);
    if ( !(1024<=PORTO_TURMAS  &&  PORTO_TURMAS<=65535) ) {
        // make sure ports are valid
        printf("!!!INVALID ARGUMENTS!!!\n-> <PORTO_TURMAS> must be integer between 1024-65535\n");
        return 1;
    }



    bzero((void *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
    addr.sin_port = htons((short)atoi(argv[2]));

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("!!!ERROR!!!\n-> Could not open client side socket.\n");
        exit(1);
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("!!!ERROR!!!\n-> Could not connect to server.\n");
        exit(1);
    }


    char buffer_in[BUF_SIZE];       // um buffer para guardar msgs de entrada
    char auxbuffer_in[BUF_SIZE];
    char *arg;
    char buffer_out[BUF_SIZE];      // um buffer para escrever as msgs de saida
    int nread;

    while (1) {
        nread = read(fd, buffer_in, BUF_SIZE - 1);      // }
        buffer_in[nread-1] = '\0';                      // } recieve response (and welcome message) from server
        //printf("FROM SERVER: \"%s\"\n", buffer_in);     // }
        strcpy(auxbuffer_in, buffer_in);

        arg = strtok(auxbuffer_in, " ");
        if ( strcmp(arg, "-+!SERVER-CL0SING!+-")==0 ) {
            printf("SERVER CLOSED\n");
            break;

        } else if ( strcmp(arg, "-+!L0G0UT!+-")==0 ) {
            // leave all multicast groups
            log_out();
        } else if ( strcmp(arg, "-+!MULT1C4ST!+-")==0 ) {
            // start multicast listener + skip waiting for user input
            arg = strtok(NULL, " ");        // ip multicast
            // look for empty slot in mtlisteners_state
            for (int i=0; i<N_CLASSES; i++) {
                if (mtlisteners[i].state==0) {
                    mtlisteners[i].state = 1;
                    strcpy(mtlisteners[i].addr, arg);
                    pthread_create(&mtlisteners[i].listener_thread, NULL, multicast_listener, (void *)&i);
                    break;
                }
            }

        } else {
            strcpy(auxbuffer_in, buffer_in);
            arg = strtok(auxbuffer_in, "^");
            printf("%s", arg);
            arg = strtok(NULL, "^");
            strcpy(header, arg);
            printf("\n\n%s", header);

            fgets(buffer_out, BUF_SIZE-1, stdin);                           // }
            buffer_out[strlen(buffer_out)-1] = '\0';    // remove '\n'         }
            write(fd, buffer_out, 1 + strlen(buffer_out));                  // } send request to server
            //printf("TO SERVER: \"%s\"\n", buffer_out);                      // }
        }
    }

    close(fd);
    for (int i=0; i<N_CLASSES; i++) {
        if (mtlisteners[i].state==1) {
            pthread_cancel(mtlisteners[i].listener_thread);
        }
    }

    return 0;
}





void handle_sigint() {
    write(fd, "-+!QUIT!+-", 1+strlen("-+!QUIT!+-"));
    printf("\n-> Closing Client\n");
    close(fd);
    log_out();
    exit(0);
}

void *multicast_listener( void *arg ) {
    int id = *(int*)arg;

    mtlisteners[id].socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (mtlisteners[id].socketfd == -1) {
        printf("!!!ERROR!!!\n-> Could not open client side socket.\n");
        return NULL;
    }

    int reuse = 1;
    if (setsockopt(mtlisteners[id].socketfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("!!!ERROR!!!\n-> Could not set socket options.\n");
        close(mtlisteners[id].socketfd);
        return NULL;
    }
    

    struct sockaddr_in multicast_addr_struct;
    bzero((void *)&multicast_addr_struct, sizeof(multicast_addr_struct));
    multicast_addr_struct.sin_family = AF_INET;
    multicast_addr_struct.sin_addr.s_addr = htonl(INADDR_ANY);
    //printf("[DEBUG] SUPOSED PORT: %d\n", 5000 + inet_addr(multicast_addr)%1000);
    multicast_addr_struct.sin_port = htons( 5000 + inet_addr(mtlisteners[id].addr)%1000 );

    if (bind(mtlisteners[id].socketfd, (struct sockaddr *)&multicast_addr_struct, sizeof(multicast_addr_struct)) < 0) {
        printf("!!!ERROR!!!\n-> Could not bind socket.\n");
        close(mtlisteners[id].socketfd);
        return NULL;
    }

    // join multicast group
    mtlisteners[id].group.imr_multiaddr.s_addr = inet_addr(mtlisteners[id].addr);
    mtlisteners[id].group.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(mtlisteners[id].socketfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mtlisteners[id].group, sizeof(mtlisteners[id].group)) < 0) {
        printf("!!!ERROR!!!\n-> Could not join multicast group.\n");
        close(mtlisteners[id].socketfd);
        return NULL;
    }


    char buffer_in[BUF_SIZE];
    int nread;
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    //printf("[DEBUG] Listening to multicast <%s:%d>\n", multicast_addr, multicast_addr_struct.sin_port);
    while (1) {
        buffer_in[0] = '\0';
        //printf("[DEBUG] Waiting for multicast message\n");
        if ( (nread=recvfrom(mtlisteners[id].socketfd, buffer_in, BUF_SIZE-1, 0, (struct sockaddr *)&server_addr, &server_addr_len))<0 ) {
            printf("!!!ERROR!!!\n-> Could not recieve multicast message.\n");
            break;
        }
        buffer_in[nread] = '\0';
        printf("\x1b[2F\x1b[0J");       // move cursor to start of previous line and clear lines in front
        //printf("FROM MULTICAST: \"%s\"\n", buffer_in);
        printf("%s\n", buffer_in);
        printf("\n\n%s", header);
        fflush(stdout);
    }

    if (setsockopt(mtlisteners[id].socketfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mtlisteners[id].group, sizeof(mtlisteners[id].group)) < 0) {
        printf("!!!ERROR!!!\n-> Could not leave multicast group.\n");
        close(mtlisteners[id].socketfd);
        return NULL;
    }

    close(mtlisteners[id].socketfd);
    return NULL;
}

int leave_multicast(int id) {
    if (mtlisteners[id].state==0) {
        return 0;
    }
    pthread_cancel(mtlisteners[id].listener_thread);

    if (setsockopt(mtlisteners[id].socketfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mtlisteners[id].group, sizeof(mtlisteners[id].group)) < 0) {
        printf("!!!ERROR!!!\n-> Could not leave multicast group.\n");
        close(mtlisteners[id].socketfd);
        return -1;
    }
    //printf("LEAVING MULTICAST <%s>\n", mtlisteners[id].addr);
    mtlisteners[id].state = 0;
    mtlisteners[id].addr[0] = '\0';
    mtlisteners[id].socketfd = -1;
    bzero((void *)&mtlisteners[id].group, sizeof(mtlisteners[id].group));

    close(mtlisteners[id].socketfd);
    return 0;
}

void log_out() {
    for (int i=0; i<N_CLASSES; i++) {
        leave_multicast(i);
    }
}
