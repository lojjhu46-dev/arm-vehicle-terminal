package com.example.camerareceiverpublic;

/**
 * 纯组帧状态机：把一系列已校验通过的 UDP/WS 分片重组为完整 JPEG 帧。
 *
 * 组帧逻辑与 CameraReceiver 原工程 UdpReceiver.java（以及 C 端 recv_thread）逐行对应：
 *  - 仅 packet_id == 0 的包可作为一帧起点；
 *  - 帧号必须连续（last + 1，uint32 回绕），否则跳过异常帧；
 *  - 片序号必须严格递增（expectedPacket），乱序/重复/属性变化 -> 丢弃整帧；
 *  - 校验失败（损坏包）由调用方在 feed 前通过 CameraPacket.isValid() 拦截；
 *  - 只有长度完整的 JPEG 帧才被返回交给解码端。
 *
 * 本类不依赖任何网络组件，可单测。
 */
public class FrameAssembler {

    private static final long UINT32_MASK = 0xFFFFFFFFL;

    /* 组帧状态（对应 UdpReceiver.java 局部变量） */
    private boolean haveFrame = false;
    private boolean haveLastFrame = false;
    private long activeFrame = 0;
    private long lastFrame = 0;
    private int frameSize = 0;
    private int packetTotal = 0;
    private int expectedPacket = 0;
    private int receivedSize = 0;
    private byte[] jpegBuf = null;

    /** 喂入一个已校验通过的分片；返回完整 JPEG 帧字节或 null。 */
    public byte[] feed(CameraPacket packet) {
        if (!haveFrame) {
            // 仅包号 0 可作为一帧起点
            if (packet.packetId != 0) return null;
            // 帧号不连续时跳过异常帧
            if (haveLastFrame && packet.frameId != ((lastFrame + 1) & UINT32_MASK)) {
                lastFrame = packet.frameId;
                return null;
            }
            // 根据首包声明的帧大小分配组帧空间
            jpegBuf = new byte[packet.frameSize];
            activeFrame = packet.frameId;
            frameSize = packet.frameSize;
            packetTotal = packet.packetTotal;
            expectedPacket = 0;
            receivedSize = 0;
            haveFrame = true;
        }

        // 乱序、重复或帧属性变化均丢弃当前帧
        if (packet.frameId != activeFrame || packet.frameSize != frameSize
                || packet.packetTotal != packetTotal || packet.packetId != expectedPacket
                || packet.packetSize > frameSize - receivedSize) {
            boolean nextFrame = packet.packetId == 0
                    && packet.frameId == ((activeFrame + 1) & UINT32_MASK);
            dropFrame();
            // 当前包恰为下一帧首包时立即重新组帧（对应 C 端 goto process_packet）
            if (nextFrame) return feed(packet);
            return null;
        }

        // 顺序拼接有效负载
        System.arraycopy(packet.packetbuf, 0, jpegBuf, receivedSize, packet.packetSize);
        receivedSize += packet.packetSize;
        expectedPacket++;
        if (expectedPacket != packetTotal) return null;

        // 仅长度完整的 JPEG 帧才返回
        byte[] result = null;
        if (receivedSize == frameSize) {
            result = jpegBuf;
        }
        lastFrame = activeFrame;
        haveLastFrame = true;
        haveFrame = false;
        jpegBuf = null;
        return result;
    }

    /** 丢弃正在组装的整帧（对应 C 端损坏包分支 / UdpReceiver.dropFrame）。 */
    private void dropFrame() {
        if (haveFrame) {
            lastFrame = activeFrame;
            haveLastFrame = true;
        }
        haveFrame = false;
        jpegBuf = null;
    }

    /** 断线重连时调用，清空所有组帧状态，避免跨连接拼接出半帧。 */
    public void reset() {
        haveFrame = false;
        haveLastFrame = false;
        activeFrame = 0;
        lastFrame = 0;
        frameSize = 0;
        packetTotal = 0;
        expectedPacket = 0;
        receivedSize = 0;
        jpegBuf = null;
    }
}
