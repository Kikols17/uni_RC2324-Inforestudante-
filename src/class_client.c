#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <signal.h>

#define BUF_SIZE 1024


void handle_sigint();


int fd;


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
    char buffer_out[BUF_SIZE];      // um buffer para escrever as msgs de saida
    int nread;

    while (1) {
        nread = read(fd, buffer_in, BUF_SIZE - 1);      // }
        buffer_in[nread-1] = '\0';                      // } recieve response (and welcome message) from server
        //printf("FROM SERVER: \"%s\"\n", buffer_in);     // }

        if ( strcmp(buffer_in, "-+!SERVER-CL0SING!+-")==0 ) {
            printf("SERVER CLOSED\n");
            break;
        } else {
            printf("\n%s", buffer_in);
        }

        fgets(buffer_out, BUF_SIZE-1, stdin);                           // }
        buffer_out[strlen(buffer_out)-1] = '\0';    // remove '\n'         }
        write(fd, buffer_out, 1 + strlen(buffer_out));                  // } send request to server
        //printf("TO SERVER: \"%s\"\n", buffer_out);                      // }
    }

    close(fd);

    return 0;
}





void handle_sigint() {
    write(fd, "-+!QUIT!+-", 1+strlen("-+!QUIT!+-"));
    printf("\n-> Closing Client\n");
    close(fd);
    exit(0);
}