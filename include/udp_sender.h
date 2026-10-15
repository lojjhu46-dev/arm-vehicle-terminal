#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "jpeglib.h"

// 整合自 camera_project：UDP 图形帧发送协议常量（与接收端 relay_server 逐字段一致）。
#define UDP_PACKET_DATA_SIZE 1300 // 每分片 JPEG 负载最大字节数
#define UDP_MAX_JPEG_SIZE (2 * 1024 * 1024) // 单帧 JPEG 大小上限

// 与 camera_project 的 PacketInfo_t 一致：24 字节帧头(6×uint32，小端) + 1300 字节负载。
typedef struct udp_packet_info {
    uint32_t frame_id;      // 帧号，每发送一帧自增
    uint32_t frame_size;    // 完整 JPEG 帧字节数
    uint32_t packet_id;     // 分片序号，从 0 开始
    uint32_t packet_total;  // 一帧总分片数
    uint32_t packet_size;   // 当前分片有效负载字节数
    uint32_t checksum;      // 负载校验和，与 camera_project packet_checksum 一致
    unsigned char packetbuf[UDP_PACKET_DATA_SIZE];
} UdpPacketInfo_t;

// UDP 发送队列槽位状态。
#define UDP_SLOT_FREE 0
#define UDP_SLOT_QUEUED 1
#define UDP_SLOT_SENDING 2

// 独立 UDP 发送线程状态：摄像头线程提交 YUYV 帧，发送线程异步编码并分片发送。
typedef struct {
    int udp_fd;                        // UDP socket 文件描述符
    struct sockaddr_in dest_addr;      // 对端接收地址
    pthread_t worker_thread;           // 后台编码/发送线程
    pthread_mutex_t mutex;             // 保护队列和状态
    pthread_cond_t work_ready;         // 通知后台线程有新帧
    JSAMPLE *rgb_buffer;               // 复用的 RGB 转换缓冲（一帧）
    unsigned char **slots;             // YUYV 帧队列缓冲数组
    int *slot_states;                  // 各槽位状态（FREE/QUEUED/SENDING）
    unsigned int next_slot;            // 下次优先提交的槽位
    unsigned int next_worker_slot;     // 下次优先取出的槽位
    unsigned int dropped_frames;       // 队列满时的丢帧统计
    unsigned int frame_id;             // 本地发送帧号
    int worker_stop;                   // 请求后台线程退出
    int worker_started;                // 后台线程已创建标志
    int ready;                         // 初始化全部完成标志（保护 destroy 安全重复调用）
} UdpSender;

int udp_sender_init(UdpSender *sender); // 创建 UDP socket 并启动发送线程
void udp_sender_destroy(UdpSender *sender); // 停止发送线程并回收资源
int udp_sender_submit_frame(UdpSender *sender, const void *yuyv); // 非阻塞提交一帧 YUYV
unsigned int udp_sender_dropped_frames(const UdpSender *sender); // 查询丢帧数

#endif
