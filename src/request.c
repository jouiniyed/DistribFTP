#include "csapp.h"
#include "request.h"
#include "filereader.h"

#define MAX_SLAVES 10
static char g_slave_ips[MAX_SLAVES][64];
static int  g_slave_ports[MAX_SLAVES];
static int  g_nb_slaves = 0;

void set_slaves(char ips[][64], int ports[], int nb) {
    g_nb_slaves = (nb < MAX_SLAVES) ? nb : MAX_SLAVES;
    for (int i = 0; i < g_nb_slaves; i++) {
        strncpy(g_slave_ips[i], ips[i], 63);
        g_slave_ips[i][63] = '\0';
        g_slave_ports[i] = ports[i];
    }
}

static void propagate_rm(char *nom, char *ip, int port) {
    // lowercase open_clientfd returns -1 on failure instead of exiting
    int fd = open_clientfd(ip, port);
    if (fd < 0) return;

    request_t req;
    memset(&req, 0, sizeof(request_t));
    req.type = RM;
    strncpy(req.nom, nom, 255);
    req.nom[255] = '\0';
    req.propagate = 1;

    rio_writen(fd, &req, sizeof(request_t));

    response_t res;
    rio_t rio;
    Rio_readinitb(&rio, fd);
    Rio_readnb(&rio, &res, sizeof(response_t));

    request_t bye;
    memset(&bye, 0, sizeof(request_t));
    bye.type = BYE;
    rio_writen(fd, &bye, sizeof(request_t));

    Close(fd);
}

static void propagate_put(char *nom, char *ip, int port) {
    int fd = open_clientfd(ip, port);
    if (fd < 0) return;

    request_t req;
    memset(&req, 0, sizeof(request_t));
    req.type = PUT;
    strncpy(req.nom, nom, 255);
    req.nom[255] = '\0';
    req.propagate = 1;

    rio_writen(fd, &req, sizeof(request_t));

    response_t res;
    rio_t rio;
    Rio_readinitb(&rio, fd);
    Rio_readnb(&rio, &res, sizeof(response_t));
    if (res.code != SUCCES) {
        Close(fd);
        return;
    }

    char path[512];
    strcpy(path, "dirServer/");
    strcat(path, nom);

    int ffd = open(path, O_RDONLY);
    if (ffd < 0) {
        Close(fd);
        return;
    }

    size_t file_size = lseek(ffd, 0, SEEK_END);
    lseek(ffd, 0, SEEK_SET);
    size_t nb_blocs = (file_size + 512 - 1) / 512;
    rio_writen(fd, &nb_blocs, sizeof(size_t));

    char buf[512];
    ssize_t n;
    size_t sent = 0;
    while (sent < nb_blocs) {
        n = Rio_readn(ffd, buf, 512);
        if (n < 512) memset(buf + n, 0, 512 - n);
        rio_writen(fd, buf, 512);
        sent++;
    }
    close(ffd);

    request_t bye;
    memset(&bye, 0, sizeof(request_t));
    bye.type = BYE;
    rio_writen(fd, &bye, sizeof(request_t));

    Close(fd);
}

request_t* init_request(typereq_t type, char nom[256]) {
    request_t *r = (request_t *)calloc(1, sizeof(request_t));
    if (!r) return NULL;

    r->type = type;
    if (nom) {
        strncpy(r->nom, nom, 255);
        r->nom[255] = '\0';
    }

    return r;
}

void setType(request_t *r, typereq_t t) { if (r != NULL) r->type = t; }

void setNom(request_t *r, char *nom) {
    if (r == NULL || nom == NULL) return;
    strncpy(r->nom, nom, 255);
    r->nom[255] = '\0';
}

typereq_t getType(request_t *r) {
    if (r != NULL) return r->type;
    return UNKNOWN;
}

char* getNom(request_t *r) {
    if (r != NULL) return r->nom;
    return NULL;
}

response_t requestHandler(int connfd) {
    ssize_t n;
    rio_t rio;
    request_t req;
    response_t res;

    Rio_readinitb(&rio, connfd);

    while ((n = Rio_readnb(&rio, &req, sizeof(request_t))) > 0) {
        if (req.type == GET) {
            res = filereader(connfd, getNom(&req), req.offset);
        } else if (req.type == LS) {
            res = filels(connfd);
        } else if (req.type == RM) {
            res = filerm(connfd, getNom(&req));
            if (res.code == SUCCES && req.propagate == 0) {
                for (int i = 0; i < g_nb_slaves; i++)
                    propagate_rm(getNom(&req), g_slave_ips[i], g_slave_ports[i]);
            }
        } else if (req.type == PUT) {
            res = fileput(connfd, getNom(&req), &rio);
            if (res.code == SUCCES && req.propagate == 0) {
                for (int i = 0; i < g_nb_slaves; i++)
                    propagate_put(getNom(&req), g_slave_ips[i], g_slave_ports[i]);
            }
        } else if (req.type == BYE) {
            res.code = SUCCES;
            break;
        } else {
            res.code = ERREUR;
        }
    }

    return res;
}
