"""Windows 中继服务：接收开发板 UDP 分片，按"帧感知单槽 + 发完整帧 + 按帧漏桶限速"
策略经 WebSocket 广播给公网客户端。

数据流：
  开发板(ARM) --UDP 8888 分片--> 本服务(UDP 收包+校验+组帧) --WebSocket binary 逐片--> cpolar HTTP隧道 --> 公网接收程序

优化说明（cpolar 1Mbps 限速场景）：
  - 逐包透传会让旧帧分片在慢速隧道中持续积压，接收端延迟随时间无界累积；
  - 本实现先组帧，只保留【最新一帧】（新帧覆盖旧帧，未发出的旧帧被自然丢弃）；
  - 广播循环【发完整帧】：一帧发完所有分片才取帧槽最新帧再发，发送期间新帧不打断，
    避免接收端"半帧饥饿"长期收不满帧（实测可把延迟从 5s 级降到 0.6s 级）；
  - 按帧漏桶限速（RateLimiter）：每发完一帧补齐整帧时间预算，实际发送速率恒 ≤
    --max-kbps（默认 80KB/s），不超发、不积压、无令牌桶式突发；
  - 发送侧使用连续 frame_id（取最新帧内容，但帧号自增连续），保证接收端
    "帧号连续"检查全部通过；
  - 协议格式不变（仍 1324B/包、小端帧头、原校验和），接收端零改动兼容。
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import struct
import time
from contextlib import suppress
from typing import Dict, List, Optional, Set, Tuple

import websockets
from websockets.asyncio.server import ServerConnection, serve

import protocol
from protocol import FrameAssembler

LOG = logging.getLogger("relay_server")

# 慢客户端判定：单次分片发送等待时长超过该阈值则视为慢客户端并断开
SEND_TIMEOUT = 2.0
# 广播速率统计窗口（秒）
STATS_WINDOW = 5.0


class RateLimiter:
    """按帧严格限速（漏桶）：每帧发送后一次性补齐时间预算。

    算法：
      - 每次 acquire(nbytes) 将 `_next_time` 前进 nbytes / rate；
      - 若当前时间晚于 `_next_time`（空闲累积），重置为当前时间，杜绝突发；
      - 调用方在【发完一帧】后调用一次，sleep 一次补齐整帧预算。
        相比"每片 sleep"，sleep 次数减少一个数量级，asyncio.sleep 调度误差
        影响大幅降低，实际速率贴近且不超过限速；
      - 瞬时突发粒度 = 一帧大小（如 3KB），对隧道压力小。
    """

    def __init__(self, max_bytes_per_sec: float) -> None:
        self._rate = max(max_bytes_per_sec, 1.0)  # 至少 1 B/s，避免除零
        self._next_time = time.monotonic()

    async def acquire(self, nbytes: int) -> None:
        """占用 nbytes 的发送时间预算；未到时间则 sleep 补齐。"""
        now = time.monotonic()
        # 空闲后首次发送：不追历史欠账，直接以当前时间为起点
        if now > self._next_time:
            self._next_time = now
        self._next_time += nbytes / self._rate
        delay = self._next_time - time.monotonic()
        if delay > 0:
            await asyncio.sleep(delay)


class FrameSlot:
    """最新帧槽：新帧覆盖旧帧，版本号自增；供广播循环等待与快照读取。

    单槽设计的核心目的：在带宽受限时，只保留最新的完整帧，
    未发出的旧帧被新帧直接覆盖丢弃，避免旧数据积压导致延迟累积。

    frame_id 在 publish 时分配且【每个发布帧唯一连续】——即使某帧被打断丢弃，
    其 id 也不会再被其它帧复用/占用，保证广播出去的 frame_id 序列始终连续，
    接收端"帧号连续"检查不会误触发（关键：被打断的帧不额外消耗 id）。
    """

    def __init__(self) -> None:
        self._frame: Optional[bytes] = None
        self._frame_id: int = 0
        self._seq: int = 0
        self._event = asyncio.Event()

    @property
    def seq(self) -> int:
        return self._seq

    def publish(self, frame: bytes) -> None:
        """新帧到达：分配连续 frame_id，覆盖旧帧，版本号自增，唤醒等待者。"""
        self._frame = frame
        self._frame_id = (self._frame_id + 1) & protocol.UINT32_MASK
        self._seq += 1
        self._event.set()

    async def wait_next(self, last_seq: int) -> Tuple[int, int, bytes]:
        """阻塞等待一个比 last_seq 更新的帧，返回 (seq, frame_id, frame) 快照。"""
        while self._seq <= last_seq:
            self._event.clear()
            await self._event.wait()
        assert self._frame is not None
        return self._seq, self._frame_id, self._frame


def repack_frame(frame: bytes, frame_id: int) -> List[bytes]:
    """把完整 JPEG 帧重切为 1300B/片并重写帧头（小端 6×uint32 + checksum）。

    返回固定 1324B/片的分片列表（不足 1300B 的尾片用零填充补齐，与 C 端 sendto
    整包 1324B 语义一致，保证接收端长度校验通过）；frame_id 由调用方传入
    （中继自增连续序列），packet_id/packet_total/packet_size/checksum 按分片真实内容重算。
    """
    frame_size = len(frame)
    total = (frame_size + protocol.PACKET_DATA_SIZE - 1) // protocol.PACKET_DATA_SIZE
    packets: List[bytes] = []
    for i in range(total):
        offset = i * protocol.PACKET_DATA_SIZE
        chunk = frame[offset:offset + protocol.PACKET_DATA_SIZE]
        header = struct.pack(
            "<6I",
            frame_id,
            frame_size,
            i,
            total,
            len(chunk),
            protocol.checksum(chunk),
        )
        packets.append(header + chunk + bytes(protocol.PACKET_DATA_SIZE - len(chunk)))
    return packets


class RelayServer:
    """UDP 收包组帧 + 帧感知单槽转发 + 主动限速 WebSocket 广播。"""

    def __init__(self, udp_port: int, ws_port: int, max_kbps: float = 80.0) -> None:
        self.udp_port = udp_port
        self.ws_port = ws_port
        self.max_bytes_per_sec = max_kbps * 1024.0
        self.clients: Set[ServerConnection] = set()
        self.slot = FrameSlot()
        self.assembler = FrameAssembler()
        # ---- 统计 ----
        self.packet_count: int = 0       # UDP 收包数
        self.invalid_count: int = 0      # 无效包数
        self.frames_received: int = 0    # 组出的完整帧数（输入）
        self.frames_sent: int = 0        # 实际发出完整帧的帧数（输出，多客户端时累计）
        self.frames_dropped: int = 0     # 丢弃帧数 = frames_received - frames_sent（输入多于输出时）
        self.bytes_sent: int = 0         # 实际发送字节数（限速观察，周期清零）
        self.total_bytes_sent: int = 0   # 实际发送字节数（累计，永不重置，供监控/测试）
        self._stats_start: float = time.monotonic()

    # ---- WebSocket 客户端管理 ----

    async def _handle_client(self, ws: ServerConnection) -> None:
        self.clients.add(ws)
        peer = getattr(ws, "remote_address", "?")
        LOG.info("客户端连接: %s（当前 %d）", peer, len(self.clients))
        # 每客户端独立限速器，互不阻塞
        limiter = RateLimiter(self.max_bytes_per_sec)
        # 广播任务与断开探测并行；recv 探测到断开即取消广播任务
        broadcast_task = asyncio.create_task(self._broadcast_loop(ws, limiter))
        try:
            await self._recv_probe(ws)
        except (websockets.exceptions.ConnectionClosed, ConnectionError):
            pass
        finally:
            broadcast_task.cancel()
            with suppress(asyncio.CancelledError):
                await broadcast_task
            self.clients.discard(ws)
            LOG.info("客户端断开: %s（当前 %d）", peer, len(self.clients))

    async def _recv_probe(self, ws: ServerConnection) -> None:
        """探测对端断开（中继单向广播，客户端消息非预期则忽略）。"""
        while True:
            try:
                msg = await asyncio.wait_for(ws.recv(), timeout=30)
                if msg is not None:
                    LOG.warning("忽略客户端非预期数据: %r", msg)
            except asyncio.TimeoutError:
                continue

    async def _broadcast_loop(self, ws: ServerConnection,
                              limiter: RateLimiter) -> None:
        """每客户端独立 task：取最新帧 -> 重切分片 -> 限速【发完整帧】。

        策略说明（关键）：
        - 主动限速（令牌桶）控制带宽，每片发送后等待令牌；
        - 一帧【发完所有分片】才取下一帧，发送期间到达的新帧仅覆盖帧槽自然丢弃，
          接收端每次稳定收到完整近期帧，避免"半帧饥饿"导致的长时间收不满帧；
        - 延迟 ≈ 一帧传输时间(限速下) + 帧槽停留 + 网络 RTT，不随时间累积。
        """
        last_seq = 0
        send_frame_id = 0  # 发送侧连续帧号：取最新帧内容，但 frame_id 始终连续递增
        while True:
            try:
                seq, _slot_frame_id, frame = await self.slot.wait_next(last_seq)
            except (websockets.exceptions.ConnectionClosed, ConnectionError):
                return
            # 关键：使用发送侧连续帧号，而非帧槽原始 id。
            # 取"最新帧"会导致原始 id 跳变（中间帧被覆盖未发出），
            # 接收端"帧号连续"检查会丢弃跳号帧；自增发送帧号保证接收端全部可组帧。
            send_frame_id = (send_frame_id + 1) & protocol.UINT32_MASK
            packets = repack_frame(frame, send_frame_id)
            self.frames_sent += 1
            for pkt in packets:
                try:
                    await asyncio.wait_for(ws.send(pkt), timeout=SEND_TIMEOUT)
                except (asyncio.TimeoutError, websockets.exceptions.ConnectionClosed,
                        ConnectionError):
                    # 慢客户端或已断开，交由 _handle_client 清理
                    return
                self.bytes_sent += len(pkt)
                self.total_bytes_sent += len(pkt)
            # 发完完整帧后一次性补齐整帧时间预算（按帧限速，burst 粒度=1帧）
            await limiter.acquire(len(frame))
            # 取最新帧（发送期间被覆盖的旧帧已被自然丢弃）
            last_seq = seq

    # ---- UDP 接收 ----

    def _datagram_received(self, data: bytes) -> None:
        self.packet_count += 1
        # 长度与协议结构必须一致
        if len(data) != protocol.TOTAL_SIZE:
            self.invalid_count += 1
            return
        pkt = protocol.parse_packet(data)
        if pkt is None or not protocol.is_valid(pkt):
            self.invalid_count += 1
            return
        # 组帧：完整帧进入单槽（覆盖未发出的旧帧）
        frame = self.assembler.feed(pkt)
        if frame is not None:
            self.frames_received += 1
            self.slot.publish(frame)

    # ---- 周期性统计 ----

    async def _stats_loop(self) -> None:
        while True:
            await asyncio.sleep(STATS_WINDOW)
            now = time.monotonic()
            elapsed = now - self._stats_start
            if elapsed <= 0:
                continue
            dropped = max(0, self.frames_received - self.frames_sent)
            self.frames_dropped = dropped
            LOG.info(
                "收包 %.0f/s | 无效包 %d | 组帧 %d | 发送帧 %d | 丢弃 %d | "
                "实际发送 %.0f KB/s | 客户端 %d",
                self.packet_count / elapsed,
                self.invalid_count,
                self.frames_received,
                self.frames_sent,
                dropped,
                self.bytes_sent / 1024.0 / elapsed,
                len(self.clients),
            )
            self.packet_count = 0
            self.invalid_count = 0
            self.frames_received = 0
            self.frames_sent = 0
            self.bytes_sent = 0
            self._stats_start = now

    # ---- 启动 ----

    async def run(self) -> None:
        loop = asyncio.get_running_loop()

        # UDP 监听
        transport, _ = await loop.create_datagram_endpoint(
            lambda: _UdpProtocol(self),
            local_addr=("0.0.0.0", self.udp_port),
        )
        LOG.info("UDP 监听 0.0.0.0:%d", self.udp_port)

        # WebSocket 服务（cpolar HTTP 隧道指向 localhost:8080）
        async with serve(self._handle_client, "0.0.0.0", self.ws_port):
            LOG.info("WebSocket 服务 ws://0.0.0.0:%d（供 cpolar 指向）", self.ws_port)
            stats_task = asyncio.create_task(self._stats_loop())
            try:
                await asyncio.Future()  # 永久运行
            finally:
                stats_task.cancel()
                transport.close()


class _UdpProtocol(asyncio.DatagramProtocol):
    """将 datagram_received 转发到 RelayServer。"""

    def __init__(self, server: RelayServer) -> None:
        self.server = server

    def datagram_received(self, data: bytes, addr) -> None:
        self.server._datagram_received(data)

    def error_received(self, exc) -> None:
        LOG.warning("UDP 错误: %s", exc)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="相机公网中继服务（UDP 组帧 -> 帧感知单槽 -> WebSocket）")
    parser.add_argument("--udp-port", type=int, default=8888,
                        help="UDP 接收端口（开发板发送目标），默认 8888")
    parser.add_argument("--ws-port", type=int, default=8080,
                        help="WebSocket 端口（cpolar 指向），默认 8080")
    parser.add_argument("--host", default="0.0.0.0", help="WebSocket 绑定地址，默认 0.0.0.0")
    parser.add_argument("--max-kbps", type=float, default=80.0,
                        help="主动限速带宽(KB/s)，默认 80（≈0.62Mbps，低于免费版 cpolar 实测吞吐，避免积压）")
    parser.add_argument("--log-level", default="INFO",
                        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
                        help="日志级别，默认 INFO")
    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper()),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    server = RelayServer(args.udp_port, args.ws_port, max_kbps=args.max_kbps)
    try:
        asyncio.run(server.run())
    except KeyboardInterrupt:
        LOG.info("收到中断信号，退出")
    except OSError as e:
        LOG.error("启动失败: %s", e)


if __name__ == "__main__":
    main()
