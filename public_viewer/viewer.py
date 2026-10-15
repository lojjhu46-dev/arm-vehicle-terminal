"""公网接收程序：通过 WebSocket 连接 cpolar 公网地址，逐包解析原协议分片、
组帧、解码并实时显示，窗口显示 FPS；断线自动指数退避重连。

数据流：
  开发板(ARM) --UDP--> Windows中继 --WS--> cpolar HTTP隧道 --公网--> 本程序

本程序按与原 Android app（CameraPacket.java / UdpReceiver.java）逐行一致的逻辑
解析 1324B 分片并组帧，保证公网端收到的画面与手机直连一致。
"""
from __future__ import annotations

import argparse
import logging
import queue
import threading
import time

import cv2
import numpy as np

import protocol
from protocol import FrameAssembler, is_valid, parse_packet

LOG = logging.getLogger("viewer")

# 帧队列容量：接收快于解码时丢弃旧帧，保证实时性（与原 app QUEUE_CAPACITY=2 一致）
QUEUE_CAPACITY = 2
# 重连退避：初始 1s，上限 30s
RECONNECT_BASE = 1.0
RECONNECT_MAX = 30.0


class Viewer:
    """WS 接收线程 + 组帧 + 帧队列 + 解码显示主线程。"""

    def __init__(self, ws_url: str) -> None:
        self.ws_url = ws_url
        self.running = True
        self.frame_queue: "queue.Queue[bytes]" = queue.Queue(maxsize=QUEUE_CAPACITY)
        self.assembler = FrameAssembler()
        self.stats_lock = threading.Lock()
        self.packet_count = 0
        self.invalid_count = 0
        self.recv_frame_count = 0
        self.displayed_frame_count = 0

    # ---- WS 接收线程 ----

    def _recv_loop(self) -> None:
        import websockets.sync.client as ws_client

        backoff = RECONNECT_BASE
        while self.running:
            try:
                with ws_client.connect(self.ws_url, open_timeout=10, close_timeout=5) as conn:
                    LOG.info("已连接: %s", self.ws_url)
                    backoff = RECONNECT_BASE
                    self._handle_connection(conn)
            except Exception as e:  # 网络异常、断线、握手失败等
                if not self.running:
                    break
                LOG.warning("连接/传输异常: %s", e)
            if self.running:
                # 断线后重置组帧状态，避免跨连接拼接出半帧
                self.assembler.reset()
                LOG.info("将在 %.0fs 后重连...", backoff)
                time.sleep(backoff)
                backoff = min(backoff * 2, RECONNECT_MAX)

    def _handle_connection(self, conn) -> None:
        for raw in conn:
            if not self.running:
                break
            if isinstance(raw, str):
                # 非预期文本消息，忽略
                continue
            if len(raw) != protocol.TOTAL_SIZE:
                self._count_invalid()
                self.assembler.reset()
                continue
            pkt = parse_packet(raw)
            if pkt is None or not is_valid(pkt):
                self._count_invalid()
                self.assembler.reset()
                continue
            with self.stats_lock:
                self.packet_count += 1
            frame = self.assembler.feed(pkt)
            if frame is not None:
                with self.stats_lock:
                    self.recv_frame_count += 1
                # 队列满则丢弃旧帧，保证实时性
                if self.frame_queue.full():
                    try:
                        self.frame_queue.get_nowait()
                    except queue.Empty:
                        pass
                self.frame_queue.put(frame)

    def _count_invalid(self) -> None:
        with self.stats_lock:
            self.invalid_count += 1

    # ---- 解码显示线程 ----

    def _display_loop(self) -> None:
        win = "CameraReceiver (公网) [q 退出]"
        cv2.namedWindow(win, cv2.WINDOW_NORMAL)
        fps_start = time.monotonic()
        fps_tick = 0
        last_status = ""
        try:
            while self.running:
                try:
                    jpeg = self.frame_queue.get(timeout=0.5)
                except queue.Empty:
                    if not self.running:
                        break
                    self._render_status(win, last_status)
                    if cv2.waitKey(1) == ord("q"):
                        break
                    continue

                img = cv2.imdecode(np.frombuffer(jpeg, dtype=np.uint8), cv2.IMREAD_COLOR)
                if img is not None:
                    self.displayed_frame_count += 1
                    fps_tick += 1
                    fps = self._fps(fps_start, fps_tick)
                    last_status = (f"{self.ws_url} | {fps:.0f} FPS | "
                                   f"收帧 {self.recv_frame_count} | 无效包 {self.invalid_count}")
                    cv2.imshow(win, img)
                else:
                    last_status = f"{self.ws_url} | JPEG 解码失败"
                    self._render_status(win, last_status)
                if cv2.waitKey(1) == ord("q"):
                    break
        finally:
            cv2.destroyAllWindows()

    @staticmethod
    def _fps(start: float, ticks: int) -> float:
        elapsed = time.monotonic() - start
        return ticks / elapsed if elapsed > 0 else 0.0

    def _render_status(self, win: str, text: str) -> None:
        canvas = np.zeros((40, 640, 3), dtype=np.uint8)
        cv2.putText(canvas, text, (10, 26), cv2.FONT_HERSHEY_SIMPLEX,
                    0.6, (255, 255, 255), 1, cv2.LINE_AA)
        cv2.imshow(win, canvas)

    # ---- 周期统计 ----

    def _stats_loop(self) -> None:
        while self.running:
            time.sleep(5)
            with self.stats_lock:
                pkt, inv = self.packet_count, self.invalid_count
                self.packet_count, self.invalid_count = 0, 0
            LOG.info("速率: %d 包/s | 无效包: %d | 已组帧: %d",
                     pkt // 5, inv, self.recv_frame_count)

    # ---- 启动 ----

    def run(self) -> None:
        threads = [
            threading.Thread(target=self._recv_loop, name="ws-recv", daemon=True),
            threading.Thread(target=self._display_loop, name="display", daemon=True),
            threading.Thread(target=self._stats_loop, name="stats", daemon=True),
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join()


def main() -> None:
    parser = argparse.ArgumentParser(description="相机公网画面接收程序")
    parser.add_argument("--url", default="ws://214006ed.r27.cpolar.top",
                        help="公网 WebSocket 地址（cpolar 隧道），默认 ws://214006ed.r27.cpolar.top")
    parser.add_argument("--log-level", default="INFO",
                        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
                        help="日志级别，默认 INFO")
    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper()),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    LOG.info("启动公网接收程序，目标 %s(按 q 退出画面窗口）", args.url)
    Viewer(args.url).run()


if __name__ == "__main__":
    main()
