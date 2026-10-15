"""端到端延迟测量：量化"开发板发帧时刻 -> 接收端完整收到"的延迟。

原理：每帧内容前 4 字节写入【发送时刻(ms)】，接收端收到完整帧后读出时间戳，
计算 (接收时刻 - 发送时刻) 即端到端延迟。

用法：py relay_server/test_latency.py
（内部启动中继 + 灌帧 + WS 客户端组帧，无需外部依赖）
"""
from __future__ import annotations

import asyncio
import logging
import socket
import struct
import threading
import time
from typing import List, Tuple

import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import protocol as pr
import relay_server as rs

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
LOG = logging.getLogger("latency_test")

UDP_PORT = 18887
WS_PORT = 18087
FRAME_BYTES = 3000       # 每帧约 3 片（≈30KB，模拟开发板 q55 帧）
MAX_KBPS = 100.0         # 中继限速
FEED_PERIOD = 0.02       # 灌帧周期 20ms（≈50fps，超限速触发丢帧）
FEED_DURATION = 5.0      # 灌帧持续


def make_frame(now_ms: int) -> bytes:
    """前 4 字节放发送时刻(ms)。"""
    return struct.pack(">I", now_ms) + bytes(FRAME_BYTES - 4)


def feed_one(sock: socket.socket, now_ms: int, frame_id: int) -> None:
    frame = make_frame(now_ms)
    total = (len(frame) + pr.PACKET_DATA_SIZE - 1) // pr.PACKET_DATA_SIZE
    for i in range(total):
        off = i * pr.PACKET_DATA_SIZE
        chunk = frame[off:off + pr.PACKET_DATA_SIZE]
        raw = bytearray(pr.TOTAL_SIZE)
        # frame_id 必须连续递增（与开发板一致），否则中继 FrameAssembler 按帧号连续检查丢弃
        struct.pack_into('<6I', raw, 0, frame_id, len(frame), i, total,
                         len(chunk), pr.checksum(chunk))
        raw[pr.HEADER_SIZE:pr.HEADER_SIZE + len(chunk)] = chunk
        sock.sendto(bytes(raw), ('127.0.0.1', UDP_PORT))


def main() -> None:
    srv = rs.RelayServer(udp_port=UDP_PORT, ws_port=WS_PORT, max_kbps=MAX_KBPS)
    threading.Thread(target=lambda: asyncio.run(srv.run()), daemon=True).start()
    time.sleep(1.0)

    import websockets.sync.client as ws_client

    received: List[Tuple[float, int]] = []  # (接收时刻s, 发送时刻ms)
    asm = pr.FrameAssembler()

    def reader() -> None:
        with ws_client.connect(f'ws://127.0.0.1:{WS_PORT}') as c:
            for raw in c:
                pkt = pr.parse_packet(raw)
                if pkt is None or not pr.is_valid(pkt):
                    continue
                frame = asm.feed(pkt)
                if frame is not None:
                    received.append((time.monotonic(), struct.unpack(">I", frame[:4])[0]))

    rt = threading.Thread(target=reader, daemon=True)
    rt.start()
    time.sleep(1.0)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    fed = 0
    start = time.monotonic()
    while time.monotonic() - start < FEED_DURATION:
        # 存相对偏移毫秒（避免绝对时间戳超 uint32 溢出）；frame_id 递增保持连续
        feed_one(sock, fed * int(FEED_PERIOD * 1000), fed)
        fed += 1
        time.sleep(FEED_PERIOD)
    sock.close()
    time.sleep(2.0)  # 等中继把限速节奏下的帧发完

    LOG.info("灌入 %d 帧，收到 %d 帧 | 中继: 组帧 %d 发送 %d 丢弃 %d",
             fed, len(received), srv.frames_received, srv.frames_sent, srv.frames_dropped)

    if not received:
        LOG.warning("未收到任何帧！")
        return

    # 计算每帧端到端延迟（秒）：发送偏移(ms)/1000 = 相对 start 的秒数
    delays = []
    for recv_t, send_ms in received:
        delays.append(recv_t - (start + send_ms / 1000.0))

    delays.sort()
    n = len(delays)
    LOG.info("端到端延迟(秒): 最小 %.2f | 中位 %.2f | P90 %.2f | P95 %.2f | 最大 %.2f",
             delays[0], delays[n // 2], delays[int(n * 0.9)],
             delays[int(n * 0.95)], delays[-1])

    # 最新一帧的延迟（代表当前实时延迟，而非历史积压）
    last_send_ms = received[-1][1]
    last_recv_t = received[-1][0]
    last_latency = last_recv_t - (start + last_send_ms / 1000.0)
    LOG.info("最后一帧延迟: %.2fs（代表当前实时延迟）", last_latency)


if __name__ == "__main__":
    main()
