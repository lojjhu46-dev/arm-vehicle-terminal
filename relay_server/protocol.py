"""共享协议模块：与开发板 C 端 clinet_and_server.h / Android 端 CameraPacket.java 逐字段一致。

一包固定 1324 字节 = 24 字节帧头(6×uint32) + 1300 字节 JPEG 负载。
帧头 6 字段为【小端】字节序（C 端直接赋值未调 htonl），故用 struct.unpack('<6I') 读取。
"""
from __future__ import annotations

import struct
from typing import Dict, Optional

# ---- 协议常量（与 C 端 clinet_and_server.h 完全一致）----
PACKET_DATA_SIZE = 1300
HEADER_SIZE = 24  # 6 × uint32
TOTAL_SIZE = HEADER_SIZE + PACKET_DATA_SIZE  # 1324
MAX_JPEG_SIZE = 2 * 1024 * 1024  # 帧容量上限，与 C 端一致
UINT32_MASK = 0xFFFFFFFF  # 帧号/校验和按 uint32 处理，溢出回绕

# 解析后的字段名（对应 PacketInfo_t 成员 / CameraPacket 属性）
F_FRAME_ID = "frame_id"
F_FRAME_SIZE = "frame_size"
F_PACKET_ID = "packet_id"
F_PACKET_TOTAL = "packet_total"
F_PACKET_SIZE = "packet_size"
F_CHECKSUM = "checksum"
F_PAYLOAD = "payload"


def checksum(data: bytes) -> int:
    """(sum<<5)-sum+byte 逐字节，取低 32 位，与 C 端 packet_checksum 完全一致。"""
    sum_: int = 0
    for b in data:
        sum_ = ((sum_ << 5) - sum_ + b) & UINT32_MASK
    return sum_


def parse_packet(raw: bytes) -> Optional[Dict[str, int | bytes]]:
    """解析 1324B 分片，帧头按小端读取；长度不符返回 None。"""
    if len(raw) != TOTAL_SIZE:
        return None
    frame_id, frame_size, packet_id, packet_total, packet_size, chk = (
        struct.unpack("<6I", raw[:HEADER_SIZE])
    )
    payload = raw[HEADER_SIZE:]
    return {
        F_FRAME_ID: frame_id,
        F_FRAME_SIZE: frame_size,
        F_PACKET_ID: packet_id,
        F_PACKET_TOTAL: packet_total,
        F_PACKET_SIZE: packet_size,
        F_CHECKSUM: chk,
        F_PAYLOAD: payload,
    }


def is_valid(packet: Dict[str, int | bytes]) -> bool:
    """对应 C 端 recv_thread 的 invalid 检查（取反）：
    frame_size 范围、packet_id/total/size 合法性、checksum 一致。"""
    frame_size = packet[F_FRAME_SIZE]
    packet_total = packet[F_PACKET_TOTAL]
    packet_id = packet[F_PACKET_ID]
    packet_size = packet[F_PACKET_SIZE]
    if not (0 < frame_size <= MAX_JPEG_SIZE):
        return False
    if packet_total <= 0 or packet_id >= packet_total:
        return False
    if not (0 < packet_size <= PACKET_DATA_SIZE):
        return False
    return packet[F_CHECKSUM] == checksum(packet[F_PAYLOAD][:packet_size])  # type: ignore


class FrameAssembler:
    """组帧状态机，与 Android UdpReceiver.java 逐行对应。

    - 仅 packet_id == 0 的包可作为一帧起点；
    - 帧号必须连续（last + 1，uint32 回绕），否则跳过异常帧；
    - 片序号必须严格递增（expected_packet），乱序/重复/属性变化 -> 丢弃整帧；
    - 校验失败（损坏包）由调用方在 feed 前通过 is_valid 拦截；
    - 只有长度完整的 JPEG 帧才被返回交给解码端。
    """

    def __init__(self) -> None:
        self.have_frame: bool = False
        self.have_last_frame: bool = False
        self.active_frame: int = 0
        self.last_frame: int = 0
        self.frame_size: int = 0
        self.packet_total: int = 0
        self.expected_packet: int = 0
        self.received_size: int = 0
        self.jpeg_buf: Optional[bytearray] = None

    def _drop_frame(self, set_last: bool = True) -> None:
        """丢弃正在组装的整帧（对应 C 端损坏包分支 / UdpReceiver.dropFrame）。"""
        if set_last and self.have_frame:
            self.last_frame = self.active_frame
            self.have_last_frame = True
        self.have_frame = False
        self.jpeg_buf = None

    def feed(self, packet: Dict[str, int | bytes]) -> Optional[bytes]:
        """喂入一个已校验通过的分片，返回完整 JPEG 帧字节或 None。"""
        frame_id = packet[F_FRAME_ID]
        frame_size = packet[F_FRAME_SIZE]
        packet_total = packet[F_PACKET_TOTAL]
        packet_id = packet[F_PACKET_ID]
        packet_size = packet[F_PACKET_SIZE]
        payload = packet[F_PAYLOAD]

        if not self.have_frame:
            # 仅包号 0 可作为一帧起点
            if packet_id != 0:
                return None
            # 帧号不连续时跳过异常帧
            if self.have_last_frame and frame_id != ((self.last_frame + 1) & UINT32_MASK):
                self.last_frame = frame_id
                return None
            # 根据首包声明的帧大小分配组帧空间
            self.jpeg_buf = bytearray(frame_size)
            self.active_frame = frame_id
            self.frame_size = frame_size
            self.packet_total = packet_total
            self.expected_packet = 0
            self.received_size = 0
            self.have_frame = True

        # 乱序、重复或帧属性变化均丢弃当前帧
        if (
            frame_id != self.active_frame
            or frame_size != self.frame_size
            or packet_total != self.packet_total
            or packet_id != self.expected_packet
            or packet_size > self.frame_size - self.received_size
        ):
            next_frame = packet_id == 0 and frame_id == ((self.active_frame + 1) & UINT32_MASK)
            self._drop_frame()
            # 当前包恰为下一帧首包时立即重新组帧（对应 C 端 goto process_packet）
            if next_frame:
                return self.feed(packet)
            return None

        # 顺序拼接有效负载
        assert self.jpeg_buf is not None
        self.jpeg_buf[self.received_size:self.received_size + packet_size] = payload[:packet_size]
        self.received_size += packet_size
        self.expected_packet += 1
        if self.expected_packet != packet_total:
            return None

        # 仅长度完整的 JPEG 帧才返回
        result: Optional[bytes] = None
        if self.received_size == self.frame_size:
            result = bytes(self.jpeg_buf)
        self.last_frame = self.active_frame
        self.have_last_frame = True
        self.have_frame = False
        self.jpeg_buf = None
        return result

    def reset(self) -> None:
        """清空所有组帧状态（断线重连时调用）。"""
        self.have_frame = False
        self.have_last_frame = False
        self.active_frame = 0
        self.last_frame = 0
        self.frame_size = 0
        self.packet_total = 0
        self.expected_packet = 0
        self.received_size = 0
        self.jpeg_buf = None
