#include "csapp.h"
#include "request.h"
#include "slave_addr.h"

#define MAX_SLAVES 16
#define PORT_MASTER 2121

typedef struct {
    int fd;
    char ip[64];
    int port;
} slave_info_t;

// lowercase open_clientfd returns -1 on failure instead of exiting
static int slave_alive(slave_info_t *e) {
    int fd = open_clientfd(e->ip, e->port);
    if (fd >= 0) {
        Close(fd);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3 || (argc - 1) % 2 != 0) {
        fprintf(stderr, "usage: %s <ip1> <port1> [<ip2> <port2> ...]\n", argv[0]);
        exit(1);
    }

    int nb_slaves = (argc - 1) / 2;
    if (nb_slaves > MAX_SLAVES) {
        fprintf(stderr, "too many slaves (max %d)\n", MAX_SLAVES);
        exit(1);
    }

    slave_info_t slaves[MAX_SLAVES];

    for (int i = 0; i < nb_slaves; i++) {
        char *host = argv[1 + i * 2];
        int port   = atoi(argv[2 + i * 2]);

        slaves[i].fd = Open_clientfd(host, port);
        strncpy(slaves[i].ip, host, 63);
        slaves[i].ip[63] = '\0';
        slaves[i].port = port;
        printf("Connected to slave %d: %s:%d\n", i, slaves[i].ip, slaves[i].port);
    }

    printf("All slaves connected, ready to accept clients\n");

    int listenfd = Open_listenfd(PORT_MASTER);
    printf("Master listening on port %d\n", PORT_MASTER);

    socklen_t clientlen = (socklen_t)sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;

    int turn = 0; // round-robin index

    while (1) {
        int connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        slave_addr_t addr;
        addr.port = 0; // port=0 signals no slave available

        for (int try = 0; try < nb_slaves; try++) {
            int idx = (turn + try) % nb_slaves;
            if (slave_alive(&slaves[idx])) {
                strncpy(addr.ip, slaves[idx].ip, 63);
                addr.ip[63] = '\0';
                addr.port = slaves[idx].port;
                turn = (idx + 1) % nb_slaves;
                break;
            }
        }

        if (addr.port == 0)
            printf("No slave available\n");
        else
            printf("Client redirected to %s:%d\n", addr.ip, addr.port);

        Rio_writen(connfd, &addr, sizeof(slave_addr_t));
        Close(connfd);
    }

    return 0;
}
