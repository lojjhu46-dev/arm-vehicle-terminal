"""节流验证：验证"发完整帧 + 主动限速"策略。

背景：发完整帧策略下，中继每帧发完所有分片才取最新帧；发送期间新帧覆盖帧槽自然丢弃。
接收端每次稳定收到完整近期帧，不再"半帧饥饿"。

验证点（真实 WS 链路 + 中继自带限速）：
1. 接收端收到完整帧数量显著高于打断策略（不再半帧饥饿）
2. 收到帧 frame_id 严格连续（+1），无跳帧
3. 收到的帧内容完整（协议兼容）
4. 发送节奏受控：实际发送速率 ≈ 限速（不超限速 + 20% 容差）
5. 丢帧通过帧槽覆盖发生（frames_received > frames_sent，即输入多于输出）
"""
from __future__ import annotations

import asyncio
import logging
import socket
import struct
import threading
import time
from typing import List

import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import protocol as pr
import relay_server as rs

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
LOG = logging.getLogger("throttle_test")

UDP_PORT = 18889
WS_PORT = 18089
FRAME_BYTES = 3000       # 每帧约 3 片
MAX_KBPS = 50.0          # 中继限速（KB/s）——低于生产速率，触发丢帧
FEED_PERIOD = 0.01       # 灌帧周期 10ms（≈100fps 满帧率，远超限速）
FEED_DURATION = 3.0      # 灌帧持续时长


def make_frame(frame_id: int) -> bytes:
    """生成内容唯一的帧：前 4 字节为原始帧号标记，其余用 0 填充。

    中继会重写 frame_id，故用内容前缀唯一标识每个原始帧。
    """
    return struct.pack(">I", frame_id) + bytes(FRAME_BYTES - 4)


def feed_one(sock: socket.socket, frame_id: int) -> None:
    """按原协议把一帧切成 UDP 分片发送（模拟开发板），frame_id 递增保持连续。"""
    frame = make_frame(frame_id)
    total = (len(frame) + pr.PACKET_DATA_SIZE - 1) // pr.PACKET_DATA_SIZE
    for i in range(total):
        off = i * pr.PACKET_DATA_SIZE
        chunk = frame[off:off + pr.PACKET_DATA_SIZE]
        raw = bytearray(pr.TOTAL_SIZE)
        struct.pack_into('<6I', raw, 0, frame_id, len(frame), i, total,
                         len(chunk), pr.checksum(chunk))
        raw[pr.HEADER_SIZE:pr.HEADER_SIZE + len(chunk)] = chunk
        sock.sendto(bytes(raw), ('127.0.0.1', UDP_PORT))


def main() -> None:
    srv = rs.RelayServer(udp_port=UDP_PORT, ws_port=WS_PORT, max_kbps=MAX_KBPS)
    threading.Thread(target=lambda: asyncio.run(srv.run()), daemon=True).start()
    time.sleep(1.0)

    import websockets.sync.client as ws_client

    received_frames: List[bytes] = []
    received_ids: List[int] = []
    asm = pr.FrameAssembler()

    def reader() -> None:
        with ws_client.connect(f'ws://127.0.0.1:{WS_PORT}') as c:
            for raw in c:
                pkt = pr.parse_packet(raw)
                if pkt is None or not pr.is_valid(pkt):
                    continue
                frame = asm.feed(pkt)
                if frame is not None:
                    received_frames.append(frame)
                    received_ids.append(pkt[pr.F_FRAME_ID])

    rt = threading.Thread(target=reader, daemon=True)
    rt.start()
    time.sleep(1.0)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    fed = 0
    start = time.monotonic()
    end = start + FEED_DURATION
    while time.monotonic() < end:
        feed_one(sock, fed)
        fed += 1
        time.sleep(FEED_PERIOD)
    sock.close()

    time.sleep(2.0)  # 等中继把限速节奏下的帧发完
    real_elapsed = time.monotonic() - start

    LOG.info("灌入帧数: %d, 客户端收到完整帧数: %d", fed, len(received_frames))
    LOG.info("收到 frame_id(前5/后5): %s",
             received_ids[:5] + ["..."] + received_ids[-5:] if len(received_ids) > 10 else received_ids)
    LOG.info("中继统计: 组帧 %d, 发送帧 %d, 丢弃 %d, 发送字节 %d",
             srv.frames_received, srv.frames_sent, srv.frames_dropped, srv.bytes_sent)

    # 验证点 1：完整帧数量显著高于打断策略（不再半帧饥饿）
    #   打断策略时灌入 268 帧只收到 7 帧（≈2.6%）；完整帧策略下应收到大部分帧
    #   限速 50KB/s、帧 3KB → 理论 ~16fps，3s 内约 48 帧
    assert len(received_frames) >= fed * 0.1, \
        f"发完整帧策略下应收到较多完整帧: 收到 {len(received_frames)} < 灌入 {fed} 的10%"
    LOG.info("✅ 完整帧数量显著：灌入 %d 帧收到 %d 帧（%.0f%%）",
             fed, len(received_frames), len(received_frames) / fed * 100)

    # 验证点 2：收到帧 frame_id 严格连续
    for i in range(1, len(received_ids)):
        assert received_ids[i] == (received_ids[i-1] + 1), \
            f"帧号不连续: {received_ids}"

    # 验证点 3：内容完整（协议兼容）
    for frame in received_frames:
        assert len(frame) == FRAME_BYTES, "帧长度异常"
        orig = struct.unpack(">I", frame[:4])[0]
        assert orig < fed, f"原始帧号 {orig} 非法"
        assert frame[4:] == bytes(FRAME_BYTES - 4), "帧内容损坏"

    # 验证点 4：发送节奏受控（≤ 限速 + 20% 容差）
    actual_bps = srv.total_bytes_sent / max(real_elapsed, 0.001)
    actual_kbps = actual_bps / 1024.0
    LOG.info("实际发送速率: %.0f KB/s (限速 %d KB/s)", actual_kbps, int(MAX_KBPS))
    assert actual_kbps <= MAX_KBPS * 1.2 + 1, \
        f"实际发送 {actual_kbps:.0f}KB/s 超过限速 {MAX_KBPS}KB/s"

    # 验证点 5：丢帧通过帧槽覆盖发生（组帧 > 发送）
    assert srv.frames_received > srv.frames_sent, \
        f"生产超限速时应有帧被覆盖丢弃: 组帧 {srv.frames_received} 发送 {srv.frames_sent}"
    LOG.info("✅ 丢帧通过帧槽覆盖生效：组帧 %d 发送 %d 丢弃 %d",
             srv.frames_received, srv.frames_sent, srv.frames_dropped)

    LOG.info("✅ 节流验证全部通过：完整帧充足、帧号连续、协议兼容、发送节奏受控")


if __name__ == "__main__":
    main()
