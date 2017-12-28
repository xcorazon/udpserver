#ifndef __UDP_SERVER_H__
#define __UDP_SERVER_H__

#include <stdint.h>
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
    uint8_t  type;
    uint64_t id;
    uint32_t request_id;
    uint16_t packs_count;
    uint16_t subpack;
};
#pragma pack(pop)

#define PAYLOAD_DATA_SIZE (MAX_DATAGRAM_SIZE-sizeof(struct data_header))


#endif /* __UDP_SERVER_H__ */