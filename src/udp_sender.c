#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "app_config.h"
#include "udp_sender.h"

// 一帧 YUYV 原始数据的字节数。
#define YUYV_FRAME_BYTES (CAMERA_FRAME_W * CAMERA_FRAME_H * 2)

// 与 camera_project frame.c 的 packet_checksum 完全一致：
// (sum<<5)-sum+byte 逐字节，取低 32 位，接收端 relay_server 按同一算法校验。
static uint32_t packet_checksum(const unsigned char *data, uint32_t size)
{
    uint32_t sum = 0;

    while (size--)
        sum = (sum << 5) - sum + *data++;
    return sum;
}

// 与 camera_project frame.c 的 yuyv_to_rgb_pix 完全一致：定点整数 YUYV->RGB。
static void yuyv_to_rgb_pix(int Y, int Cb, int Cr, JSAMPLE **pimage_buffer)
{
    int r = Y + (1436 * (Cr - 128)) / 1024;
    int g = Y - (352 * (Cb - 128) + 731 * (Cr - 128)) / 1024;
    int b = Y + (1815 * (Cb - 128)) / 1024;

    if (r < 0)
        r = 0;
    else if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    else if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;
    *(*pimage_buffer)++ = r;
    *(*pimage_buffer)++ = g;
    *(*pimage_buffer)++ = b;
}

// 与 camera_project frame.c 的 write_JPEG_memory 一致：RGB 内存 JPEG 编码（质量 55）。
static int write_JPEG_memory(unsigned char **jpeg_buf, size_t *jpeg_size,
                             const JSAMPLE *image_buffer)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row[1];

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    /* JPEG 直接输出到内存，避免逐帧写文件。 */
    jpeg_mem_dest(&cinfo, jpeg_buf, jpeg_size);
    cinfo.image_width = CAMERA_FRAME_W;
    cinfo.image_height = CAMERA_FRAME_H;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 55, TRUE); // 与 camera_project 一致：降低带宽和编码压力
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height)
    {
        row[0] = (JSAMPROW)&image_buffer[cinfo.next_scanline * CAMERA_FRAME_W * 3];
        jpeg_write_scanlines(&cinfo, row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return 0;
}

// 判断队列中是否仍有待发送帧。
static int udp_sender_has_queued(const UdpSender *sender)
{
    for (unsigned int i = 0; i < UDP_QUEUE_DEPTH; ++i)
    {
        if (sender->slot_states[i] == UDP_SLOT_QUEUED)
            return 1;
    }
    return 0;
}

// 在锁保护下取出一块最早排队等待发送的 YUYV 帧槽。
static int udp_sender_take_slot_locked(UdpSender *sender, unsigned int *slot_index)
{
    for (unsigned int offset = 0; offset < UDP_QUEUE_DEPTH; ++offset)
    {
        unsigned int index = (sender->next_worker_slot + offset) % UDP_QUEUE_DEPTH;

        if (sender->slot_states[index] == UDP_SLOT_QUEUED)
        {
            sender->slot_states[index] = UDP_SLOT_SENDING;
            sender->next_worker_slot = (index + 1) % UDP_QUEUE_DEPTH;
            *slot_index = index;
            return 0;
        }
    }
    return -1;
}

// 后台线程：YUYV->RGB->JPEG 内存编码->1300B 分片->UDP 发送（逻辑与 camera_project 一致）。
static void *udp_sender_worker(void *argument)
{
    UdpSender *sender = argument;

    for (;;)
    {
        unsigned int slot_index = 0;
        unsigned char *yuyv;
        JSAMPLE *pimage_buffer;
        unsigned char *jpeg_buf = NULL;
        size_t jpeg_size = 0;
        unsigned int packet_total;
        unsigned int frame_id;
        unsigned int i;

        pthread_mutex_lock(&sender->mutex);
        while (!sender->worker_stop && !udp_sender_has_queued(sender))
            pthread_cond_wait(&sender->work_ready, &sender->mutex);
        if (sender->worker_stop)
        {
            pthread_mutex_unlock(&sender->mutex);
            break;
        }
        if (udp_sender_take_slot_locked(sender, &slot_index) == -1)
        {
            pthread_mutex_unlock(&sender->mutex);
            continue;
        }
        yuyv = sender->slots[slot_index];
        pthread_mutex_unlock(&sender->mutex);

        /* YUYV 每 4 字节含两个像素，逐对转换并写入 RGB 缓冲（同 camera_project yuv2rgb）。 */
        pimage_buffer = sender->rgb_buffer;
        for (i = 0; i < YUYV_FRAME_BYTES; i += 4)
        {
            yuyv_to_rgb_pix((unsigned char)yuyv[i], (unsigned char)yuyv[i + 1],
                            (unsigned char)yuyv[i + 3], &pimage_buffer);
            yuyv_to_rgb_pix((unsigned char)yuyv[i + 2], (unsigned char)yuyv[i + 1],
                            (unsigned char)yuyv[i + 3], &pimage_buffer);
        }

        /* 内存 JPEG 编码并按固定负载大小切成 UDP 分片发送。 */
        if (write_JPEG_memory(&jpeg_buf, &jpeg_size, sender->rgb_buffer) == 0 &&
            jpeg_size > 0)
        {
            pthread_mutex_lock(&sender->mutex);
            ++sender->frame_id;
            frame_id = sender->frame_id;
            pthread_mutex_unlock(&sender->mutex);

            packet_total = (jpeg_size + UDP_PACKET_DATA_SIZE - 1) / UDP_PACKET_DATA_SIZE;
            for (i = 0; i < packet_total; ++i)
            {
                unsigned int offset = i * UDP_PACKET_DATA_SIZE;
                unsigned int size = jpeg_size - offset;
                UdpPacketInfo_t packet;

                if (size > UDP_PACKET_DATA_SIZE)
                    size = UDP_PACKET_DATA_SIZE;
                memset(&packet, 0, sizeof(packet));
                packet.frame_id = frame_id;
                packet.frame_size = jpeg_size;
                packet.packet_id = i;
                packet.packet_total = packet_total;
                packet.packet_size = size;
                memcpy(packet.packetbuf, jpeg_buf + offset, size);
                packet.checksum = packet_checksum(packet.packetbuf, packet.packet_size);
                /* 整包 1324B 发送（含零填充尾片），接收端长度校验通过。 */
                sendto(sender->udp_fd, &packet, sizeof(packet), 0,
                       (struct sockaddr *)&sender->dest_addr, sizeof(sender->dest_addr));
            }
            free(jpeg_buf);
        }

        pthread_mutex_lock(&sender->mutex);
        sender->slot_states[slot_index] = UDP_SLOT_FREE;
        pthread_mutex_unlock(&sender->mutex);
    }
    return NULL;
}

// 创建 UDP socket、配置对端地址并启动后台发送线程。
int udp_sender_init(UdpSender *sender)
{
    struct sockaddr_in dest_addr;

    memset(sender, 0, sizeof(*sender));
    sender->udp_fd = -1;

    sender->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sender->udp_fd < 0)
        return -1;
    /* 非阻塞：目标不可达（如 ARP/路由未决）时 sendto 立即返回错误而非阻塞线程。 */
    if (fcntl(sender->udp_fd, F_SETFL, fcntl(sender->udp_fd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        close(sender->udp_fd);
        sender->udp_fd = -1;
        return -1;
    }

    /* 配置发送目标，IP/端口见 app_config.h，默认 192.168.33.53:8888。 */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_RECEIVER_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(UDP_RECEIVER_IP);
    sender->dest_addr = dest_addr;

    if (pthread_mutex_init(&sender->mutex, NULL) != 0)
    {
        close(sender->udp_fd);
        sender->udp_fd = -1;
        return -1;
    }
    if (pthread_cond_init(&sender->work_ready, NULL) != 0)
    {
        pthread_mutex_destroy(&sender->mutex);
        close(sender->udp_fd);
        sender->udp_fd = -1;
        return -1;
    }

    sender->rgb_buffer = malloc(CAMERA_FRAME_W * CAMERA_FRAME_H * 3);
    if (sender->rgb_buffer == NULL)
    {
        pthread_cond_destroy(&sender->work_ready);
        pthread_mutex_destroy(&sender->mutex);
        close(sender->udp_fd);
        sender->udp_fd = -1;
        return -1;
    }

    sender->slots = calloc(UDP_QUEUE_DEPTH, sizeof(*sender->slots));
    sender->slot_states = calloc(UDP_QUEUE_DEPTH, sizeof(*sender->slot_states));
    if (sender->slots == NULL || sender->slot_states == NULL)
    {
        free(sender->slots);
        free(sender->slot_states);
        free(sender->rgb_buffer);
        pthread_cond_destroy(&sender->work_ready);
        pthread_mutex_destroy(&sender->mutex);
        close(sender->udp_fd);
        sender->udp_fd = -1;
        return -1;
    }
    for (unsigned int i = 0; i < UDP_QUEUE_DEPTH; ++i)
    {
        sender->slots[i] = malloc(YUYV_FRAME_BYTES);
        if (sender->slots[i] == NULL)
        {
            for (unsigned int j = 0; j < i; ++j)
                free(sender->slots[j]);
            free(sender->slots);
            free(sender->slot_states);
            free(sender->rgb_buffer);
            pthread_cond_destroy(&sender->work_ready);
            pthread_mutex_destroy(&sender->mutex);
            close(sender->udp_fd);
            sender->udp_fd = -1;
            return -1;
        }
    }

    if (pthread_create(&sender->worker_thread, NULL, udp_sender_worker, sender) != 0)
    {
        for (unsigned int i = 0; i < UDP_QUEUE_DEPTH; ++i)
            free(sender->slots[i]);
        free(sender->slots);
        free(sender->slot_states);
        free(sender->rgb_buffer);
        pthread_cond_destroy(&sender->work_ready);
        pthread_mutex_destroy(&sender->mutex);
        close(sender->udp_fd);
        sender->udp_fd = -1;
        return -1;
    }
    sender->worker_started = 1;
    sender->ready = 1;
    return 0;
}

// 停止后台线程并回收全部预申请缓冲（支持安全重复清理）。
void udp_sender_destroy(UdpSender *sender)
{
    /* 初始化未完成时（如 socket 创建失败）结构内无有效同步对象，直接清空。 */
    if (!sender->ready)
    {
        memset(sender, 0, sizeof(*sender));
        return;
    }
    if (sender->worker_started)
    {
        pthread_mutex_lock(&sender->mutex);
        sender->worker_stop = 1;
        pthread_cond_signal(&sender->work_ready);
        pthread_mutex_unlock(&sender->mutex);
        pthread_join(sender->worker_thread, NULL);
    }
    if (sender->slots != NULL)
    {
        for (unsigned int i = 0; i < UDP_QUEUE_DEPTH; ++i)
            free(sender->slots[i]);
        free(sender->slots);
    }
    free(sender->slot_states);
    free(sender->rgb_buffer);
    if (sender->udp_fd != -1)
        close(sender->udp_fd);
    pthread_cond_destroy(&sender->work_ready);
    pthread_mutex_destroy(&sender->mutex);
    memset(sender, 0, sizeof(*sender));
}

// 非阻塞提交一帧 YUYV；队列满时只记录丢帧，返回 1。
int udp_sender_submit_frame(UdpSender *sender, const void *yuyv)
{
    pthread_mutex_lock(&sender->mutex);
    if (!sender->worker_started)
    {
        pthread_mutex_unlock(&sender->mutex);
        return 0;
    }
    for (unsigned int offset = 0; offset < UDP_QUEUE_DEPTH; ++offset)
    {
        unsigned int index = (sender->next_slot + offset) % UDP_QUEUE_DEPTH;

        if (sender->slot_states[index] == UDP_SLOT_FREE)
        {
            memcpy(sender->slots[index], yuyv, YUYV_FRAME_BYTES);
            sender->slot_states[index] = UDP_SLOT_QUEUED;
            sender->next_slot = (index + 1) % UDP_QUEUE_DEPTH;
            pthread_cond_signal(&sender->work_ready);
            pthread_mutex_unlock(&sender->mutex);
            return 0;
        }
    }
    sender->dropped_frames++;
    pthread_mutex_unlock(&sender->mutex);
    return 1;
}

// 返回因队列占满而丢弃的 YUYV 帧数。
unsigned int udp_sender_dropped_frames(const UdpSender *sender)
{
    unsigned int dropped_frames;

    pthread_mutex_lock(&((UdpSender *)sender)->mutex);
    dropped_frames = sender->dropped_frames;
    pthread_mutex_unlock(&((UdpSender *)sender)->mutex);
    return dropped_frames;
}
