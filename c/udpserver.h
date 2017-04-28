#ifndef __UDP_SERVER_H__
#define __UDP_SERVER_H

#include <time.h>
#include "consts.h"

enum {
    T_OK = 0,
    T_ERR,
    T_DATA,
    T_CONNECT
};


#pragma pack(push, 1)
struct data_header
{
    unsigned char type;
    unsigned long long id;
    unsigned int request_id;
    unsigned short packs_count;
    unsigned short subpack;
};
#pragma pack(pop) 

struct udp_client
{
    unsigned long long id;
    unsigned int request_id;
    
    clock_t last_recv;
    clock_t timeout;
    clock_t pack_timeout;
    clock_t subpack_timeout;
    
    struct sockaddr addr;
    
    char recvd_subpacks[MAX_SUBPACKS_COUNT];
    char sended_subpacks[MAX_SUBPACKS_COUNT];
};

#endif /* __UDP_SERVER_H__ */