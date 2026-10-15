//最新改动
#include "clinet_and_server.h"
#include "frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int udp_fd = -1;                    // UDP套接字
struct sockaddr_in dest_addr;       // 对端地址

/* 对分片内容求校验和，检测数据损坏 */
uint32_t packet_checksum(const unsigned char *data, uint32_t size)
{
    uint32_t sum = 0;
    while (size--)
        sum = (sum << 5) - sum + *data++;
    return sum;
}

int client_and_server_init(unsigned int *lcd_mp)
{
    pthread_t tid;
    struct sockaddr_in local_addr;

    /* 创建并绑定本地视频端口 */
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        perror("socket");
        return -1;
    }
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(udp_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind");
        close(udp_fd);
        udp_fd = -1;
        return -1;
    }
    /* 配置发送目标，IP按实际对端修改（公网中继：Windows主机局域网IP） */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = inet_addr("192.168.33.54");
    /* 接收与采集并行，避免阻塞本地画面 */
    if (pthread_create(&tid, NULL, recv_thread, lcd_mp) != 0) {
        perror("pthread_create");
        close(udp_fd);
        udp_fd = -1;
        return -1;
    }
    pthread_detach(tid);
    return 0;
}

void *recv_thread(void *arg)
{
    unsigned int *lcd_mp = arg;     // LCD显存首地址
    PacketInfo_t packet;            // 单个UDP分片
    unsigned char *jpeg_buf = NULL; // 当前完整JPEG帧缓存
    /* 当前组帧状态和上一帧号 */
    uint32_t active_frame = 0, expected_packet = 0, last_frame = 0;
    uint32_t frame_size = 0, packet_total = 0, received_size = 0;
    int have_frame = 0, have_last_frame = 0;

    for (;;) {
        /* UDP按报文接收，长度必须与协议结构一致 */
        ssize_t len = recvfrom(udp_fd, &packet, sizeof(packet), 0, NULL, NULL);
        int invalid = len != (ssize_t)sizeof(packet) ||
                      packet.frame_size == 0 || packet.frame_size > MAX_JPEG_SIZE ||
                      packet.packet_total == 0 || packet.packet_id >= packet.packet_total ||
                      packet.packet_size == 0 || packet.packet_size > PACKET_DATA_SIZE ||
                      packet.checksum != packet_checksum(packet.packetbuf,
                                                         packet.packet_size);
        if (invalid) {
            /* 损坏包直接放弃正在组装的整帧 */
            if (have_frame) {
                last_frame = active_frame;
                have_last_frame = 1;
            }
            have_frame = 0;
            free(jpeg_buf);
            jpeg_buf = NULL;
            continue;
        }

    process_packet:
        if (!have_frame) {
            /* 仅包号0可作为一帧起点 */
            if (packet.packet_id != 0) continue;
            /* 帧号不连续时跳过异常帧 */
            if (have_last_frame && packet.frame_id != last_frame + 1) {
                last_frame = packet.frame_id;
                continue;
            }
            /* 根据首包声明的帧大小分配组帧空间 */
            jpeg_buf = malloc(packet.frame_size);
            if (jpeg_buf == NULL) continue;
            active_frame = packet.frame_id;
            frame_size = packet.frame_size;
            packet_total = packet.packet_total;
            expected_packet = 0;
            received_size = 0;
            have_frame = 1;
        }

        if (packet.frame_id != active_frame || packet.frame_size != frame_size ||
            packet.packet_total != packet_total || packet.packet_id != expected_packet ||
            packet.packet_size > frame_size - received_size) {
            /* 乱序、重复或帧属性变化均丢弃当前帧 */
            int next_frame = packet.packet_id == 0 &&
                             packet.frame_id == active_frame + 1;
            last_frame = active_frame;
            have_last_frame = 1;
            have_frame = 0;
            free(jpeg_buf);
            jpeg_buf = NULL;
            /* 当前包恰为下一帧首包时立即重新组帧 */
            if (next_frame) goto process_packet;
            continue;
        }

        /* 顺序拼接有效负载 */
        memcpy(jpeg_buf + received_size, packet.packetbuf, packet.packet_size);
        received_size += packet.packet_size;
        if (++expected_packet != packet_total) continue;
        /* 仅长度完整的JPEG帧才交给解码显示 */
        if (received_size == frame_size)
            read_JPEG_data(jpeg_buf, frame_size, lcd_mp);
        last_frame = active_frame;
        have_last_frame = 1;
        have_frame = 0;
        free(jpeg_buf);
        jpeg_buf = NULL;
    }
}
//最新改动
