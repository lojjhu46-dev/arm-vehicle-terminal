//最新改动
#include "frame.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static uint32_t frame_id;                  // 本地发送帧号
static unsigned int remote_frame[640 * 480]; // 对端离屏显示缓冲

/* libjpeg错误跳转，防止损坏JPEG退出程序 */
static void jpeg_error_exit(j_common_ptr cinfo)
{
    struct my_error_mgr *err = (struct my_error_mgr *)cinfo->err;
    longjmp(err->setjmp_buffer, 1);
}

int read_JPEG_data(const unsigned char *jpeg_buf, unsigned int jpeg_size,
                   unsigned int *lcd_mp)
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    JSAMPARRAY line;
    unsigned int dst_w, dst_h, y;

    /* 使用内存JPEG源，避免接收端文件读写 */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg_buf, jpeg_size);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    /* 对端画面限制在左侧640x480并保持比例 */
    dst_w = cinfo.output_width > 640 ? 640 : cinfo.output_width;
    dst_h = cinfo.output_height > 480 ? 480 : cinfo.output_height;
    if (dst_w * cinfo.output_height > dst_h * cinfo.output_width)
        dst_w = dst_h * cinfo.output_width / cinfo.output_height;
    else
        dst_h = dst_w * cinfo.output_height / cinfo.output_width;

    /* 先在离屏缓冲解码，LCD始终保留上一完整帧 */
    memset(remote_frame, 0, sizeof(remote_frame));

    line = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE,
                                      cinfo.output_width * 3, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned int src_y = cinfo.output_scanline;
        unsigned int dst_y;
        unsigned int x;
        jpeg_read_scanlines(&cinfo, line, 1);
        dst_y = src_y * dst_h / cinfo.output_height;
        if (dst_y >= dst_h) continue;
        for (x = 0; x < dst_w; ++x) {
            unsigned int src_x = x * cinfo.output_width / dst_w;
            unsigned char *rgb = &line[0][src_x * 3];
            /* RGB转ARGB，写入离屏帧 */
            remote_frame[dst_y * 640 + x] =
                (0xffU << 24) | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    /* 整帧解码后快速刷新，减少闪烁 */
    for (y = 0; y < LCD_HEIGHT; ++y)
        memcpy(&lcd_mp[y * LCD_WIDTH], &remote_frame[y * 640],
               640 * sizeof(*lcd_mp));
    return 0;
}

static int write_JPEG_memory(unsigned char **jpeg_buf, size_t *jpeg_size)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row[1];

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    /* JPEG直接输出到内存，避免每帧写文件 */
    jpeg_mem_dest(&cinfo, jpeg_buf, jpeg_size);
    cinfo.image_width = CAMERA_FRAME_W;
    cinfo.image_height = CAMERA_FRAME_H;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 55, TRUE); // 降低带宽和编码压力
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        row[0] = &image_buffer[cinfo.next_scanline * CAMERA_FRAME_W * 3];
        jpeg_write_scanlines(&cinfo, row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return 0;
}

int yuyv_to_rgb_pix(int y, int cb, int cr)
{
    /* 定点YUV转RGB，同时写入JPEG编码缓冲 */
    int r = y + (1436 * (cr - 128)) / 1024;
    int g = y - (352 * (cb - 128) + 731 * (cr - 128)) / 1024;
    int b = y + (1815 * (cb - 128)) / 1024;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    *pimage_buffer++ = r;
    *pimage_buffer++ = g;
    *pimage_buffer++ = b;
    return (r << 16) | (g << 8) | b;
}

void yuv2rgb(char *pbuffer, unsigned int *lcd_mp)
{
    PacketInfo_t packet;
    unsigned char *jpeg_buf = NULL;
    size_t filesize = 0;
    unsigned int packet_total;
    unsigned int i, j;

    frame_id++; // 每次采集生成一个新的发送帧号
    if (image_buffer == NULL)
        image_buffer = malloc(CAMERA_FRAME_W * CAMERA_FRAME_H * 3);
    if (image_buffer == NULL) return;
    pimage_buffer = image_buffer;
    /* YUYV每4字节含两个像素 */
    for (i = 0, j = 0; j < CAMERA_FRAME_W * CAMERA_FRAME_H; i += 4, j += 2) {
        unsigned int pixel = yuyv_to_rgb_pix((unsigned char)pbuffer[i],
                                              (unsigned char)pbuffer[i + 1],
                                              (unsigned char)pbuffer[i + 3]);
        yuyv_to_rgb_pix((unsigned char)pbuffer[i + 2],
                        (unsigned char)pbuffer[i + 1],
                        (unsigned char)pbuffer[i + 3]);
        /* 每隔4个像素采样，形成右上角160x120本地预览 */
        if ((j / CAMERA_FRAME_W) % 4 == 0 && j % 4 == 0)
            lcd_mp[(j / CAMERA_FRAME_W / 4) * LCD_WIDTH + 640 + j % CAMERA_FRAME_W / 4] = pixel;
    }

    /* 内存JPEG按固定负载大小切成UDP分片 */
    if (write_JPEG_memory(&jpeg_buf, &filesize) == 0 && filesize > 0) {
        packet_total = (filesize + PACKET_DATA_SIZE - 1) / PACKET_DATA_SIZE;
        for (i = 0; i < packet_total; ++i) {
            unsigned int offset = i * PACKET_DATA_SIZE;
            unsigned int size = filesize - offset;
            if (size > PACKET_DATA_SIZE) size = PACKET_DATA_SIZE;
            memset(&packet, 0, sizeof(packet));
            packet.frame_id = frame_id;
            packet.frame_size = filesize;
            packet.packet_id = i;
            packet.packet_total = packet_total;
            packet.packet_size = size;
            memcpy(packet.packetbuf, jpeg_buf + offset, size);
            packet.checksum = packet_checksum(packet.packetbuf, packet.packet_size);
            sendto(udp_fd, &packet, sizeof(packet), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        }
        free(jpeg_buf);
    }
}

void mainloop(unsigned int *lcd_mp)
{
    struct v4l2_buffer buf;
    for (;;) {
        fd_set fds;
        struct timeval tv = {2, 0};
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        /* 等待摄像头产生新帧，超时后继续等待 */
        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        /* 出队、编码发送、显示本地预览，再重新入队 */
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) continue;
        yuv2rgb(buffers[buf.index].start, lcd_mp);
        ioctl(fd, VIDIOC_QBUF, &buf);
    }
}
//最新改动
