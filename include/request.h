#ifndef _REQUEST_
#define _REQUEST_

#include <stdlib.h>
#include <string.h>

#include "typereq.h"
#include "response.h"

typedef struct request request_t;

struct request {
    typereq_t type;
    char nom[256];
    size_t offset;   // blocks already received by client (0 = fresh download)
    int propagate;   // 1 = already propagated, skip; 0 = forward to other slaves
};

request_t* init_request(typereq_t type, char nom[256]);

void setType(request_t *r, typereq_t t);
void setNom(request_t *r, char *nom);

typereq_t getType(request_t *r);
char* getNom(request_t *r);

response_t requestHandler(int connfd);

void set_slaves(char ips[][64], int ports[], int nb);

#endif
