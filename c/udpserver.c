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

/* server clients */
static struct udp_client clients[MAX_CLIENTS];
static char *clients_recv_heap = 0;

/* temporary function for generate new client id */
/* TODO: заменить указателем на функцию генерации id */
static long long int get_new_id()
{
    static long long int next_id = 0;
    next_id++;
    return next_id;
}


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


static struct udp_client* get_client(unsigned long long id)
{
    printf("get client id=%i\n", (int)id);
    for(int i=0; i<MAX_CLIENTS; i++) {
        printf("client id = %i\n", (int)clients[i].id);
        if(clients[i].id == id)
            return &clients[i];
    }
    
    return 0;
}


static struct udp_client* add_client(unsigned long long id)
{
    printf("add client id=%i\n", (int)id);
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].id == 0) {
            clients[i].id = id;
            clients[i].recv_id = 1;
            clients[i].send_id = 1;
            reset_recv(&clients[i]);
            reset_send(&clients[i]);

            return &clients[i];
        }
    }

    return 0;
}


static void remove_client(unsigned long long id)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].id == id) {
            clients[i].id = 0;
            return;
        }
    }
}


static void handle_data(struct udp_client *client)
{
    client->rbuffer[client->rdata_size] = 0;
    printf("received: %s\n", client->rbuffer);
}


static void handle_ok(struct udp_client *client, struct data_header *header)
{
    if(client->send_state == RS_TRANSFER) {
        if(client->send_id == header->request_id) {
            client->sended_subpacks[header->subpack] = 1;
            if(check_send(client, header->packs_count)) {
                reset_send(client);
                client->send_id++;
            }
        }
    } else if (client->send_state == RS_CONNECTION) {
        client->send_state = RS_COMPLETED;
    }
}


void send_data(struct udp_client *client)
{
    char sbuffer[MAX_DATAGRAM_SIZE + 100];
    struct data_header *header = (struct data_header *)sbuffer;
    size_t tail = client->sdata_size % MAX_DATAGRAM_SIZE;
    
    //client->send_state = RS_TRANSFER;
    
    header->type = T_DATA;
    header->id = client->id;
    header->request_id = client->send_id;
    header->packs_count = client->sdata_size / MAX_DATAGRAM_SIZE;
    
    int i = 0;
    while(i < header->packs_count - 1) {
        header->subpack = i;
        if (!client->sended_subpacks[i]) {
            memcpy(sbuffer + sizeof(struct data_header), client->sbuffer + MAX_DATAGRAM_SIZE * i, MAX_DATAGRAM_SIZE);
            sendto(hsock, sbuffer, sizeof(struct data_header) + MAX_DATAGRAM_SIZE, 0, &client->addr, sizeof(struct sockaddr));
        }
        i++;
    }
    
    if(tail != 0) {
        header->subpack = i;
        if (!client->sended_subpacks[i]) {
            memcpy(sbuffer + sizeof(struct data_header), client->sbuffer + MAX_DATAGRAM_SIZE * i, tail);
            sendto(hsock, sbuffer, sizeof(struct data_header) + tail, 0, &client->addr, sizeof(struct sockaddr));
        }
    }
}


void send_connect(struct udp_client *client)
{
    printf("send connection id = %llu\n", client->id);
    char sbuffer[MAX_DATAGRAM_SIZE + 100];
    struct data_header *header = (struct data_header *)sbuffer;
    
    header->type = T_CONNECT;
    header->id = client->id;
    header->request_id = client->send_id;
    header->packs_count = 1;
    
    sendto(hsock, sbuffer, sizeof(struct data_header), 0, &client->addr, sizeof(struct sockaddr));

    client->send_state = RS_CONNECTION;
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
    /* check request id */
    if(header->request_id != client->recv_id)
        return;

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
    
    struct udp_client *client;

    /* check for connection request */
    if(header->type == T_CONNECT) {
        if(header->id == 0) {
            unsigned long long connection_id = get_new_id();
            client = add_client(connection_id);
            memcpy(&client->addr, addr, sizeof(struct sockaddr));
            
            if(client->send_state == RS_COMPLETED) {
                send_connect(client);
                return;
            } else {
                send_lost_packets(client);
                return;
            }
            
        }
    }
    
    client = get_client(header->id);
    if(!client && header->request_id == 1)
        client = add_client(header->id);
    
    if(!client && header->type != T_CONNECT) {
        return;
    }
    
    memcpy(&client->addr, addr, sizeof(struct sockaddr));
    
    if(header->type == T_DATA) {
        recv_datagram(client, header, data, size);

        if(header->request_id <= client->recv_id) {
            header->type = T_OK;
            sendto(hsock, header, sizeof(struct data_header), 0, addr, sizeof(struct sockaddr));
        }
    
    } else if(header->type == T_OK) {
        handle_ok(client, header);
    }
}


static void udp_cb(EV_P_ ev_io *w, int revents)
{
    socklen_t bytes = recvfrom(hsock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *) &addr, (socklen_t *) &addr_len);
    
    handle_packet(buffer, (struct sockaddr *)&addr, bytes);
}


static void init_clients()
{
    clients_recv_heap = (char *)malloc(MAX_SUBPACKS_COUNT * MAX_DATAGRAM_SIZE * MAX_CLIENTS);
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        clients[i].id = 0;
        clients[i].rbuffer = clients_recv_heap + i * MAX_SUBPACKS_COUNT * MAX_DATAGRAM_SIZE;
    }
}


static void release_clients()
{
    if(clients_recv_heap)
      free(clients_recv_heap);
}


static void timeout_cb (EV_P_ ev_timer *w, int revents)
{
    puts ("timeout");
    // this causes the innermost ev_run to stop iterating
    //ev_break (EV_A_ EVBREAK_ONE);
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i].id == 0 || clients[i].send_state == RS_COMPLETED)
            continue;
          
        send_lost_packets(&clients[i]);
    }
    
    ev_timer_set (w, 0.5, 0.);
    ev_timer_start (EV_A_ w);
}


int main(void)
{
    int port = UDP_PORT;
    puts("udp server started...");
    
    init_clients();
    
    /* настройка udp сокета на слушание */
    hsock = socket(PF_INET, SOCK_DGRAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
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