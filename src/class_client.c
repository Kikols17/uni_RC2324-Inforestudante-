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


void handle_sigint();
void *multicast_listener( void *arg );


int fd;
pthread_t mtlisteners_pthreads[N_CLASSES];
int mtlisteners_state[N_CLASSES];
int running = 1;


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
    char header[BUF_SIZE];
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

        } else if ( strcmp(arg, "-+!MULT1C4ST!+-")==0 ) {
            // start multicast listener + skip waiting for user input
            arg = strtok(NULL, " ");        // ip multicast
            // look for empty slot in mtlisteners_state
            for (int i=0; i<N_CLASSES; i++) {
                if (mtlisteners_state[i]==0) {
                    pthread_create(&mtlisteners_pthreads[i], NULL, multicast_listener, (void *)arg);
                    mtlisteners_state[i] = 1;
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
        if (mtlisteners_state[i]==1) {
            pthread_join(mtlisteners_pthreads[i], NULL);
        }
    }

    return 0;
}





void handle_sigint() {
    write(fd, "-+!QUIT!+-", 1+strlen("-+!QUIT!+-"));
    printf("\n-> Closing Client\n");
    close(fd);
    exit(0);
}

void *multicast_listener( void *arg ) {
    char multicast_addr[100];
    strcpy(multicast_addr, (char *)arg);

    int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd == -1) {
        printf("!!!ERROR!!!\n-> Could not open client side socket.\n");
        return NULL;
    }

    int reuse = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("!!!ERROR!!!\n-> Could not set socket options.\n");
        close(socketfd);
        return NULL;
    }

    struct sockaddr_in multicast_addr_struct;
    bzero((void *)&multicast_addr_struct, sizeof(multicast_addr_struct));
    multicast_addr_struct.sin_family = AF_INET;
    multicast_addr_struct.sin_addr.s_addr = htonl(INADDR_ANY);
    //printf("[DEBUG] SUPOSED PORT: %d\n", 5000 + inet_addr(multicast_addr)%1000);
    multicast_addr_struct.sin_port = htons( 5000 + inet_addr(multicast_addr)%1000 );

    if (bind(socketfd, (struct sockaddr *)&multicast_addr_struct, sizeof(multicast_addr_struct)) < 0) {
        printf("!!!ERROR!!!\n-> Could not bind socket.\n");
        close(socketfd);
        return NULL;
    }

    // join multicast group
    struct ip_mreq group;
    group.imr_multiaddr.s_addr = inet_addr(multicast_addr);
    group.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group)) < 0) {
        printf("!!!ERROR!!!\n-> Could not join multicast group.\n");
        close(socketfd);
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
        if ( (nread=recvfrom(socketfd, buffer_in, BUF_SIZE-1, 0, (struct sockaddr *)&server_addr, &server_addr_len))<0 ) {
            printf("!!!ERROR!!!\n-> Could not recieve multicast message.\n");
            break;
        }
        buffer_in[nread] = '\0';
        printf("FROM MULTICAST: \"%s\"\n", buffer_in);
    }

    if (setsockopt(socketfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &group, sizeof(group)) < 0) {
        printf("!!!ERROR!!!\n-> Could not leave multicast group.\n");
        close(socketfd);
        return NULL;
    }

    close(socketfd);
    return NULL;
}

