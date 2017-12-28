#include <stdbool.h>
#include <stdint.h>
#include <strings.h>

#include "client.h"

void init_client(UdpClient *client, uint64_t id)
{
    client->id = id;
    client->recv_id = 1;
    client->send_id = 1;
    reset_recv(client);
    reset_send(client);
}


bool check_recv(UdpClient *client, int subpacks_count)
{
    for(int i=0; i<subpacks_count; i++)
        if(!client->recvd_subpacks[i])
            return false;
    
    return true;
}


bool check_send(UdpClient *client, int subpacks_count)
{
    for(int i=0; i<subpacks_count; i++)
        if(!client->sended_subpacks[i])
            return false;
    
    return true;
}


void reset_recv(UdpClient *client)
{
    bzero(&client->recvd_subpacks, MAX_SUBPACKS_COUNT);
    client->recv_state = RS_COMPLETED;
}


void reset_send(UdpClient *client)
{
    bzero(&client->sended_subpacks, MAX_SUBPACKS_COUNT);
    client->send_state = RS_COMPLETED;
}