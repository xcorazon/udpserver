#ifndef __UDP_SERVER_H__
#define __UDP_SERVER_H__

#include <time.h>
#include "const.h"


#define   T_OK      0
#define   T_ERR     1
#define   T_DATA    2
#define   T_CONNECT 3
#define   T_ACCEPT_CONNECTION   4
#define   T_CONNECTION_ACCEPTED 5



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

#define PAYLOAD_DATA_SIZE (MAX_DATAGRAM_SIZE-sizeof(struct data_header))

/* client send-recv state */
#define RS_COMPLETED  0
#define RS_TRANSFER   1
#define RS_CONNECTION 2
#define RS_READY      3

struct udp_client
{
    unsigned long long id;
    unsigned int recv_id;
    unsigned int send_id;
    size_t rdata_size;    /* read data size */
    size_t sdata_size;    /* send data size */
    
    clock_t last_recv;
    clock_t timeout;
    clock_t pack_timeout;
    clock_t subpack_timeout;
    
    struct sockaddr addr;
    
    char recvd_subpacks[MAX_SUBPACKS_COUNT];
    char recv_state;
    char sended_subpacks[MAX_SUBPACKS_COUNT];
    char send_state;
    
    char *rbuffer;
    char *sbuffer;
};

#endif /* __UDP_SERVER_H__ */