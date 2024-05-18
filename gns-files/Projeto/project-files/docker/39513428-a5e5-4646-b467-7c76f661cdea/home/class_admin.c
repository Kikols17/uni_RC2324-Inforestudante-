#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define BUF_SIZE 1024

void handle_sigint();


int socket_fd;

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    socklen_t slen = sizeof(server_addr);

    signal(SIGINT, handle_sigint);      // used to close client correctly

    if (argc!=3) {
        // make sure start arguments are correct
        printf("!!!INVALID ARGUMENTS!!!\n-> %s <IP_ADDRESS> <PORTO_TURMAS>\n", argv[0]);
        exit(1);
    }

    // Create socket
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("!!!ERROR!!!\n-> Could not open client side socket.\n");
        exit(1);
    }

    // Set server address
    bzero((void *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((short)atoi(argv[2]));

    // Convert IP address
    if (inet_aton(argv[1], &server_addr.sin_addr) == 0) {
        printf("!!!ERROR!!!\n-> Could not convert IP address.\n");
        exit(1);
    }

    // Send message
    char buf_out[BUF_SIZE];
    char buf_in[BUF_SIZE];

    sprintf(buf_out, "-+!HELLO!+-");
    if (sendto(socket_fd, buf_out, BUF_SIZE-1, 0, (struct sockaddr *)&server_addr, slen) == -1) {
        printf("!!!ERROR!!!\n-> Could not send message.\n");
        exit(1);
    }

    while (1) {

        buf_in[0] = '\0';
        if (recvfrom(socket_fd, buf_in, BUF_SIZE-1, 0, (struct sockaddr *)&server_addr, &slen) == -1) {
            printf("!!!ERROR!!!\n-> Could not receive message.\n");
            exit(1);
        }
        printf("%s", buf_in);

        fgets(buf_out, BUF_SIZE, stdin);
        buf_out[strlen(buf_out)-1] = '\0'; // remove '\n' from end of line
        if (sendto(socket_fd, buf_out, BUF_SIZE-1, 0, (struct sockaddr *)&server_addr, slen) == -1) {
            printf("!!!ERROR!!!\n-> Could not send message.\n");
            exit(1);
        }
        if (strcmp(buf_out, "QUIT_SERVER") == 0) {
            handle_sigint();
        }
    }
    return 0;
}


void handle_sigint() {
    printf("\nClosing admin...\n");
    close(socket_fd);
    exit(0);
}