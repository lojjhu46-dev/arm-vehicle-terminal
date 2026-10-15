package com.example.camerareceiverpublic;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * 与开发板 C 端 PacketInfo_t 结构体一一对应。
 *
 * 字节序注意：
 *  - 帧头 6 个字段在 C 端是直接赋值（没有调用 htonl），线路上为【小端】字节序；
 *  - 端口使用了 htons 是大端，但端口只在 bind/connect 时由系统处理，这里无需解析。
 *  - 结构体无 padding，一包固定 1324 字节 = 24 字节头 + 1300 字节负载。
 *
 * 与 CameraReceiver 原工程 CameraPacket.java 完全一致（仅包名不同）。
 */
public class CameraPacket {

    public static final int PACKET_DATA_SIZE = 1300;
    public static final int HEADER_SIZE = 6 * 4;                 // 24 字节
    public static final int TOTAL_SIZE = HEADER_SIZE + PACKET_DATA_SIZE; // 1324
    public static final int MAX_JPEG_SIZE = 2 * 1024 * 1024;     // 与 C 端一致

    public long frameId;      // uint32：帧序号，溢出回绕
    public int frameSize;     // 整帧字节数
    public int packetId;      // 片序号，从 0 开始
    public int packetTotal;   // 总片数
    public int packetSize;    // 本片负载字节数
    public long checksum;     // uint32：负载校验和
    public final byte[] packetbuf = new byte[PACKET_DATA_SIZE];

    private CameraPacket() {
    }

    /** 解析一包（1324 字节原始数据），帧头按小端读取 */
    public static CameraPacket parse(byte[] raw) {
        ByteBuffer bb = ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN);
        CameraPacket p = new CameraPacket();
        p.frameId = bb.getInt() & 0xFFFFFFFFL;
        p.frameSize = bb.getInt();
        p.packetId = bb.getInt();
        p.packetTotal = bb.getInt();
        p.packetSize = bb.getInt();
        p.checksum = bb.getInt() & 0xFFFFFFFFL;
        System.arraycopy(raw, HEADER_SIZE, p.packetbuf, 0, PACKET_DATA_SIZE);
        return p;
    }

    /** 与 C 端 packet_checksum 完全一致：(sum<<5)-sum+byte，取低 32 位 */
    public static long checksum(byte[] data, int len) {
        long sum = 0;
        for (int i = 0; i < len; i++) {
            sum = ((sum << 5) - sum + (data[i] & 0xFF)) & 0xFFFFFFFFL;
        }
        return sum;
    }

    /** 对应 C 端 recv_thread 中的 invalid 检查 */
    public boolean isValid() {
        if (frameSize <= 0 || frameSize > MAX_JPEG_SIZE) return false;
        if (packetTotal <= 0 || packetId < 0 || packetId >= packetTotal) return false;
        if (packetSize <= 0 || packetSize > PACKET_DATA_SIZE) return false;
        return checksum == checksum(packetbuf, packetSize);
    }
}
