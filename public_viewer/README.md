# public_viewer — 公网相机画面接收程序

通过 WebSocket 连接 cpolar 公网地址，逐包接收 **原 1324B 分片协议**数据，按与
原 Android app（`CameraPacket.java` / `UdpReceiver.java`）**逐行一致**的组帧逻辑重组完整
JPEG 帧，用 OpenCV 解码并实时显示，窗口左上角显示实时 FPS；断线自动指数退避重连。

> 本程序可运行在任意有 Python 3 + OpenCV 的机器上（Windows / Linux / macOS），
> 只需能访问公网 cpolar 地址。

## 数据流向

```
开发板(ARM) --UDP--> Windows中继(relay_server) --WS--> cpolar HTTP隧道 --公网--> 本程序
```

## 依赖安装

```bat
py -m pip install -r requirements.txt
```

依赖：`websockets`、`opencv-python`、`numpy`。

## 启动

```bat
py viewer.py --url ws://214006ed.r27.cpolar.top
```

参数：

| 参数 | 默认 | 说明 |
|---|---|---|
| `--url` | `ws://214006ed.r27.cpolar.top` | 公网 WebSocket 地址（cpolar 隧道） |
| `--log-level` | `INFO` | 日志级别 |

按 `q` 或关闭窗口退出。

## 界面状态栏

窗口顶部状态栏显示 `地址 | FPS | 收帧数 | 无效包数`，便于观察链路质量。

## 联调验证

1. Windows 端已启动 `relay_server` 且 cpolar 隧道正常。
2. 运行本程序，日志显示 `已连接: ws://...`。
3. 开发板开始采集后，弹窗出现实时画面；状态栏显示 FPS 与收帧数。
4. 若长时间无画面：检查 cpolar 隧道是否指向 `localhost:8080`、relay_server 是否收到
   UDP 分片（看其日志中的 `组帧` 指标）、以及公网带宽是否足够。

## 说明

- 画面格式与原 Android app 完全一致（640×480 JPEG，q55）。
- 中继已做**帧感知单槽转发**（见 relay_server README）：免费版 cpolar 限速下画面约
  2-4fps，但始终显示**最近**画面，延迟不随时间累积。
- 若在无显示器环境运行，可结合 `cv2.imwrite` 保存帧做离线验证。
