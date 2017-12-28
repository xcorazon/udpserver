#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

#include "const.h"

//#include <sys/socket.h>

/* client send-recv state */
#define RS_COMPLETED  0
#define RS_TRANSFER   1
#define RS_CONNECTION 2
#define RS_READY      3

typedef struct UdpClient
{
    uint64_t id;
    uint32_t recv_id;
    uint32_t send_id;
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
    
    void *rbuffer;
    void *sbuffer;
} UdpClient;

void init_client(UdpClient *client, uint64_t id);
bool check_recv(UdpClient *client, int subpacks_count);
bool check_send(UdpClient *client, int subpacks_count);
void reset_recv(UdpClient *client);
void reset_send(UdpClient *client);

#endif /*__CLIENT_H__*/