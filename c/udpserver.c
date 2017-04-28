#include <stdio.h>
#include <strings.h>
#include <ev.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "const.h"
#include "udpserver.h"

#define BUFF_SIZE 4096


int hsock;
struct sockaddr_in addr;
int addr_len = sizeof(addr);
char buffer[BUFF_SIZE];

/* server clients */
struct udp_client clients[MAX_CLIENTS];


static struct udp_client* get_client(unsigned long long id)
{
    for(var i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].id == id)
            return &clients[i];
    }
    
    return 0;
}


static void reset_recv(struct udp_client *client)
{
    bzero(&client->recvd_subpacks, MAX_SUBPACKS_COUNT);
}


static void reset_send(struct udp_client *client)
{
    bzero(&client->sended_subpacks, MAX_SUBPACKS_COUNT);
}


struct udp_client* add_client(unsigned long long id)
{
    for(var i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].id == 0) {
            clients[i].id = id;
            clients[i].request_id = 1;
            reset_recv(&clients[i]);
            reset_send(&clients[i]);

            return &clients[i];
        }
    }

    return 0;
}


static void handle_packet(char *buffer, struct sockaddr *addr)
{
    struct data_header *header = (struct data_header *)buffer;
    struct udp_client *client = get_client(header->id);

    if(!client && header->request_id == 1) {
        client = add_client(header->id);
    }
}

static void udp_cb(EV_P_ ev_io *w, int revents)
{
    socklen_t bytes = recvfrom(hsock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *) &addr, (socklen_t *) &addr_len);
    
    buffer[bytes] = "\0";
    
    printf("message: %s", buffer);
    
    sendto(hsock, buffer, bytes, 0, (struct sockaddr *) &addr, sizeof(addr));
}


int main(void)
{
    int port = UDP_PORT;
    puts("udp server started...");
    
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
    ev_io_init(&udp_watcher, udp_cb, hsock, EV_READ);
    ev_io_start(loop, &udp_watcher);
    ev_loop(loop, 0);
    
    close(hsock);
    return 0;
}
