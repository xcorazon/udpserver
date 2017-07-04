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


void send_lost_packets()
{
  
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


static void timeout_cb (EV_P_ ev_timer *w, int revents)
{
    puts ("timeout");
    // this causes the innermost ev_run to stop iterating
    //ev_break (EV_A_ EVBREAK_ONE);
    ev_timer_set (w, 0.5, 0.);
    ev_timer_start (EV_A_ w);
}


int main(void)
{
    int port = UDP_PORT;
    puts("udp client started...");
    
    init_client();
    
    /* настройка udp сокета на слушание */
    hsock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(hsock < 0) {
        perror("socket");
        exit(2);
    }
    
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ;
    if(bind(hsock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        perror("bind");
    
    /* инициализация наблюдателя за событием */
    struct ev_loop *loop = EV_DEFAULT;
    ev_io udp_watcher;
    ev_timer timeout_watcher;
    
    ev_io_init(&udp_watcher, udp_cb, hsock, EV_READ);
    ev_io_start(loop, &udp_watcher);
    
    ev_timer_init (&timeout_watcher, timeout_cb, 0.5, 0.);
    ev_timer_start (loop, &timeout_watcher);
    
    ev_loop(loop, 0);
    
    close(hsock);
    release_clients();
    
    return 0;
}