#ifndef CLI_AND_SERV_H
#define CLI_AND_SERV_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>

//最新改动
#define PORT 8888
#define PACKET_DATA_SIZE 1300
#define MAX_JPEG_SIZE (2 * 1024 * 1024)

typedef struct packet_info
{
    uint32_t frame_id;
    uint32_t frame_size;
    uint32_t packet_id;
    uint32_t packet_total;
    uint32_t packet_size;
    uint32_t checksum;
    unsigned char packetbuf[PACKET_DATA_SIZE];
} PacketInfo_t;

extern int udp_fd;
extern struct sockaddr_in dest_addr;

int client_and_server_init(unsigned int *lcd_mp);
void *recv_thread(void *arg);
uint32_t packet_checksum(const unsigned char *data, uint32_t size);
//最新改动

#endif
