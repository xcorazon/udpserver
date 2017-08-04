#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <ev.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "const.h"
#include "udpserver.h"

#define BUFF_SIZE 4096


int hsock;
struct sockaddr_in addr;
int addr_len = sizeof(addr);
char buffer[BUFF_SIZE];


static struct udp_client client;
static char *client_recv_buf = 0;
static char *client_send_buf = 0;

static char *host = '54.165.59.155';


static int check_recv(struct udp_client *client, int subpacks_count)
{
    for(int i=0; i<subpacks_count; i++)
        if(!client->recvd_subpacks[i])
            return 0;
    
    return 1;
}


static int check_send(struct udp_client *client, int subpacks_count)
{
    for(int i=0; i<subpacks_count; i++)
        if(!client->sended_subpacks[i])
            return 0;
    
    return 1;
}


static void reset_recv(struct udp_client *client)
{
    bzero(&client->recvd_subpacks, MAX_SUBPACKS_COUNT);
    client->recv_state = RS_COMPLETED;
}


static void reset_send(struct udp_client *client)
{
    bzero(&client->sended_subpacks, MAX_SUBPACKS_COUNT);
    client->send_state = RS_COMPLETED;
}


static void handle_data(struct udp_client *client)
{
    client->rbuffer[client->rdata_size] = 0;
    printf("received: %s\n", client->rbuffer);
}


static void handle_ok(struct udp_client *client, struct data_header *header)
{
    if(client->send_id == header->request_id) {
        client->sended_subpacks[header->subpack] = 1;
        
        if(check_send(client, header->packs_count)) {
            reset_send(client);
            client->send_id++;
        }
    }
}


void send_data(struct udp_client *client, char *data, size_t size)
{
    char sbuffer[MAX_DATAGRAM_SIZE + 100];
    struct data_header *header = (struct data_header *)sbuffer;
    size_t tail = size % MAX_DATAGRAM_SIZE;
    
    client->sdata_size = size;
    client->send_state = RS_TRANSFER;
    client->sbuffer = data;
    
    header->type = T_DATA;
    header->id = client->id;
    header->request_id = client->send_id;
    header->packs_count = size / MAX_DATAGRAM_SIZE;
    
    int i = 0;
    while(i < header->packs_count - 1) {
        header->subpack = i;
        memcpy(sbuffer + sizeof(struct data_header), data + MAX_DATAGRAM_SIZE * i, MAX_DATAGRAM_SIZE);
        sendto(hsock, sbuffer, sizeof(struct data_header) + MAX_DATAGRAM_SIZE, 0, &client->addr, sizeof(struct sockaddr));
        i++;
    }
    if(tail != 0) {
        header->subpack = i;
        memcpy(sbuffer + sizeof(struct data_header), data + MAX_DATAGRAM_SIZE * i, tail);
        sendto(hsock, sbuffer, sizeof(struct data_header) + tail, 0, &client->addr, sizeof(struct sockaddr));
    }
}


void send_lost_packets(struct udp_client *client)
{
    if(client->send_state == RS_TRANSFER)
        send_data(client);
    else if(client->send_state == RS_CONNECTION)
        send_connect(client);
}


static void recv_datagram(struct udp_client *client, struct data_header *header, char *data, socklen_t size)
{
    if(client->recvd_subpacks[header->subpack])
        return;
    
    client->recvd_subpacks[header->subpack] = 1;
    
    int dgram_size = size - sizeof(struct data_header);
    memcpy(client->rbuffer + header->subpack * MAX_DATAGRAM_SIZE, data, dgram_size);
    
    /* вычисляем полный размер получаемых данных */
    if(header->subpack == header->packs_count - 1)
        client->rdata_size = (header->packs_count - 1) * MAX_DATAGRAM_SIZE + dgram_size;
    
    if(check_recv(client, header->packs_count)) {
        reset_recv(client);
        client->recv_id++;
        handle_data(client);
    }
    
}


static void handle_packet(char *buffer, struct sockaddr *addr, socklen_t size)
{
    struct data_header *header = (struct data_header *)buffer;
    char *data = buffer + sizeof(struct data_header);
    
    struct udp_client *client = get_client(header->id);
    if(!client && header->request_id == 1)
        client = add_client(header->id);
    if(!client)
        return;
    
    memcpy(&client->addr, addr, sizeof(struct sockaddr));
    
    if(header->type == T_DATA) {
        recv_datagram(client, header, data, size);
        
        header->type = T_OK;
        sendto(hsock, header, sizeof(struct data_header), 0, addr, sizeof(struct sockaddr));
        
    } else if(header->type = T_OK) {
        handle_ok(client, header);
    }
}


static void udp_cb(EV_P_ ev_io *w, int revents)
{
    socklen_t bytes = recvfrom(hsock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *) &addr, (socklen_t *) &addr_len);
    
    handle_packet(buffer, (struct sockaddr *)&addr, bytes);
}


static void init_client()
{
    client_recv_buf = (char *)malloc(MAX_SUBPACKS_COUNT * MAX_DATAGRAM_SIZE);
    client_send_buf = (char *)malloc(MAX_SUBPACKS_COUNT * MAX_DATAGRAM_SIZE);
    
    client.id = 0;

    client.recv_id = 1;
    client.send_id = 1;
    reset_recv(&client);
    reset_send(&client);

    client.rbuffer = client_recv_buf;
    cliend.sbuffer = client_send_buf;
}


static void release_client()
{
    if(client_recv_buf)
      free(client_recv_buf);
}


int main(void)
{
    struct sockaddr_in si_other;
    int s, slen=sizeof(si_other);
    char buf[BUFLEN];
    char message[BUFLEN];
    WSADATA wsa;
 
    //Initialise winsock
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Initialised.\n");
     
    //create socket
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
    {
        printf("socket() failed with error code : %d" , WSAGetLastError());
        exit(EXIT_FAILURE);
    }
     
    //setup address structure
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(UDP_PORT);
    si_other.sin_addr.S_un.S_addr = inet_addr(host);
     
    //start communication
    while(1)
    {
        printf("Enter message : ");
        gets(message);
         
        //send the message
        if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen) == SOCKET_ERROR)
        {
            printf("sendto() failed with error code : %d" , WSAGetLastError());
            exit(EXIT_FAILURE);
        }
         
        //receive a reply and print it
        //clear the buffer by filling null, it might have previously received data
        memset(buf,'\0', BUFLEN);
        //try to receive some data, this is a blocking call
        if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == SOCKET_ERROR)
        {
            printf("recvfrom() failed with error code : %d" , WSAGetLastError());
            exit(EXIT_FAILURE);
        }
         
        puts(buf);
    }
 
    closesocket(s);
    WSACleanup();
 
    return 0;
}