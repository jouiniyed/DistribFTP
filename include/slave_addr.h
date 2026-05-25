#ifndef _SLAVE_ADDR_
#define _SLAVE_ADDR_

typedef struct {
    char ip[64];
    int port;
} slave_addr_t;

#endif
