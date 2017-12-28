#include "const.h"
#include "udpserver.h"
#include "client.h"

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <ev.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>

#define BUFF_SIZE 4096


int hsock;
struct sockaddr_in addr;
int addr_len = sizeof(addr);
char buffer[BUFF_SIZE];

/* server clients */
static UdpClient clients[MAX_CLIENTS];
static char *clients_recv_buffer = 0;

/* temporary function for generate new client id */
/* TODO: заменить указателем на функцию генерации id */
static uint64_t get_new_id()
{
    static uint64_t next_id = 0;
    next_id++;
    return next_id;
}

UdpClient* get_client(uint64_t id)
{
    if (id == 0)
        return 0;
    
    printf("get client id=%" PRIu64 "\n", id);
    for(int i=0; i<MAX_CLIENTS; i++) {
        printf("client id = %" PRIu64 "\n", clients[i].id);
        if(clients[i].id == id)
            return &clients[i];
    }
    
    return NULL;
}


static UdpClient* add_client(uint64_t id)
{
    printf("add client id=%" PRIu64 "\n", id);
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].id == 0) {
            init_client(&clients[i], id);
            return &clients[i];
        }
    }

    return NULL;
}


static void remove_client(uint64_t id)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].id == id) {
            clients[i].id = 0;
            return;
        }
    }
}


static void handle_data(UdpClient *client)
{
    *(char *)(client->rbuffer + client->rdata_size) = '\0';
    printf("received: %s\n", (char *)client->rbuffer);
}


static void handle_ok(UdpClient *client, struct data_header *header)
{
    if(client->send_state == RS_TRANSFER) {
        if(client->send_id == header->request_id) {
            client->sended_subpacks[header->subpack] = 1;
            if(check_send(client, header->packs_count)) {
                reset_send(client);
                client->send_id++;
            }
        }
    } 
}


void send_datagram(int socket,
                 struct UdpClient *client,
                 char *buffer,
                 int pack_number,
                 size_t max_payload_size,
                 size_t pack_size)
{
    if (client->sended_subpacks[pack_number])
        return;
    
#define HEADER_SIZE sizeof(struct data_header)
    
    void *source_data = client->sbuffer + max_payload_size * pack_number;
    size_t data_size = HEADER_SIZE + pack_size;
    
    memcpy(buffer + HEADER_SIZE, source_data, pack_size);
    sendto(socket, buffer, data_size, 0, &client->addr, sizeof(struct sockaddr));
    
#undef HEADER_SIZE
}


void send_data(int socket, UdpClient *client)
{
    char sbuffer[MAX_DATAGRAM_SIZE + 100];
    struct data_header *header = (struct data_header *)sbuffer;
    size_t tail = client->sdata_size % PAYLOAD_DATA_SIZE;
    
    client->send_state = RS_TRANSFER;
    
    /* init header */
    header->type = T_DATA;
    header->id = client->id;
    header->request_id = client->send_id;
    
    header->packs_count = client->sdata_size / PAYLOAD_DATA_SIZE;
    if(tail)
        header->packs_count++;
    else
        tail = PAYLOAD_DATA_SIZE;
    
    /* send packets */
    int i = 0;
    int count = header->packs_count - 1;
    while(i < count) {
        header->subpack = i;
        send_datagram(socket, client, sbuffer, i, PAYLOAD_DATA_SIZE, PAYLOAD_DATA_SIZE);
        i++;
    }
    
    /* send tail (хвост данных) */
    header->subpack = i;
    send_datagram(socket, client, sbuffer, i, PAYLOAD_DATA_SIZE, tail);
}


static void send_connect(int socket, UdpClient *client)
{
    printf("send connection id = %" PRIu64 "\n", client->id);
    char sbuffer[MAX_DATAGRAM_SIZE + 100];
    struct data_header *header = (struct data_header *)sbuffer;
    
    header->type = T_ACCEPT_CONNECTION;
    header->id = client->id;
    header->request_id = client->send_id;
    header->packs_count = 1;
    
    sendto(socket, sbuffer, sizeof(struct data_header), 0, &client->addr, sizeof(struct sockaddr));

    client->send_state = RS_CONNECTION;
    client->recv_state = RS_CONNECTION;
}


static void send_lost_packets(int socket, UdpClient *client)
{
    if(client->send_state == RS_TRANSFER)
        send_data(socket, client);
    else if(client->send_state == RS_CONNECTION)
        send_connect(socket, client);
}


static void recv_datagram(UdpClient *client, struct data_header *header, char *data, socklen_t size)
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


void handle_packet(int socket, char *buffer, struct sockaddr *addr, socklen_t size)
{
    struct data_header *header = (struct data_header *)buffer;
    char *data = buffer + sizeof(struct data_header);
    
    UdpClient *client;

    /* получение клиента и проверка запроса на соединение  */
    client = get_client(header->id);
    if(header->type == T_CONNECT) {
        if(header->id == 0 && !client) {
            unsigned long long connection_id = get_new_id();
            client = add_client(connection_id);
            memcpy(&client->addr, addr, sizeof(struct sockaddr));
        }
        if (client)
            send_connect(socket, client);
        return;
    }
    if (!client)
        return;
    
    memcpy(&client->addr, addr, sizeof(struct sockaddr));
    
    if (header->type == T_ACCEPT_CONNECTION) {
        header->type = T_CONNECTION_ACCEPTED;
        sendto(hsock, header, sizeof(struct data_header), 0, addr, sizeof(struct sockaddr));
    }
    else if (header->type == T_CONNECTION_ACCEPTED) {
        client->send_state = RS_READY;
        client->recv_state = RS_READY;
    }
    else if(header->type == T_DATA) {
        recv_datagram(client, header, data, size);
        if(header->request_id <= client->recv_id) {
            header->type = T_OK;
            sendto(hsock, header, sizeof(struct data_header), 0, addr, sizeof(struct sockaddr));
        }
    }
    else if(header->type == T_OK) {
        handle_ok(client, header);
    }
}


static void udp_cb(EV_P_ ev_io *w, int revents)
{
    socklen_t bytes = recvfrom(hsock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *) &addr, (socklen_t *) &addr_len);
    
    handle_packet(hsock, buffer, (struct sockaddr *)&addr, bytes);
}


static void init_clients()
{
    clients_recv_buffer = (char *)malloc(MAX_SUBPACKS_COUNT * MAX_DATAGRAM_SIZE * MAX_CLIENTS);
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        clients[i].id = 0;
        clients[i].rbuffer = clients_recv_buffer + i * MAX_SUBPACKS_COUNT * MAX_DATAGRAM_SIZE;
    }
}


static void release_clients()
{
    if(clients_recv_buffer)
      free(clients_recv_buffer);
}


static void timeout_cb (EV_P_ ev_timer *w, int revents)
{
    puts ("timeout");
    // this causes the innermost ev_run to stop iterating
    //ev_break (EV_A_ EVBREAK_ONE);
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i].id == 0 || clients[i].send_state == RS_COMPLETED)
            continue;
          
        send_lost_packets(hsock, &clients[i]);
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