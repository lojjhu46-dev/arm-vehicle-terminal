# CameraReceiverPublic — 公网相机画面接收 App（Android）

公网接收端的**正式交付形态**：通过 WebSocket 连接 cpolar 公网隧道（默认
`ws://214006ed.r27.cpolar.top`，可在启动页修改），逐包接收中继转发的**原协议 1324B
分片**，按与原手机 App 完全一致的组帧逻辑重组 JPEG 帧并实时显示。

> 仅公网模式，不涉及局域网直连。本地联调可用 `public_viewer`（Python）或本 App 连接同一
> 公网地址。
> 对应中继：`d:/Camerareceiver/relay_server`；数据来源：开发板 → relay_server → cpolar。

## 功能

- **连接设置页**：公网 WebSocket 地址输入框（预填默认值），保存后连接，地址变更随时可改；
  地址通过 SharedPreferences 记忆，下次启动自动填充
- **画面显示页**：顶部状态栏（连接状态/错误信息）+ 中间 640×480 画面（等比居中）+
  右上角实时 FPS
- **断线自动重连**：指数退避（1s 起，上限 30s），重连期间状态栏提示，断开时清空组帧状态
- **协议完全一致**：1324B/包、小端帧头、校验和 `(sum<<5)-sum+byte`、组帧规则（仅片0起帧、
  帧号连续、片序严格递增、损坏/乱序/重包整帧丢弃）与原 App / C 端逐字段一致

## 数据流向

```
开发板(ARM) --UDP 8888分片--> relay_server(Windows中继) --WebSocket--> cpolar隧道 --公网--> 本App
```

## 目录结构

```
CameraReceiverPublic/
├── build.gradle              # 根构建：AGP 8.2.2
├── settings.gradle           # include ':app'，google/mavenCentral
├── gradle.properties
├── gradlew / gradlew.bat     # Gradle 8.2 wrapper
└── app/
    ├── build.gradle          # minSdk 21 / targetSdk 34，依赖 okhttp
    └── src/main/
        ├── AndroidManifest.xml
        ├── java/com/example/camerareceiverpublic/
        │   ├── CameraPacket.java   # 协议：常量1324B/小端解析/校验和/isValid（复用原工程）
        │   ├── FrameAssembler.java # 纯组帧状态机（从原 UdpReceiver.java 抽取）
        │   ├── WsReceiver.java     # OkHttp WS 客户端：连接/校验/组帧/重连
        │   ├── MainActivity.java   # 连接设置页
        │   └── VideoActivity.java  # 画面显示页
        └── res/                    # 布局 / drawable / values
```

## 构建（Android Studio）

# 临时设置 JAVA_HOME
$env:JAVA_HOME = "C:\Users\cxk\.jdks\jbr-17.0.14" 

# 将 Java bin 目录添加到 PATH（仅当前会话有效）
$env:PATH = "$env:JAVA_HOME\bin;$env:PATH"

# 验证
java -version

1. 用 Android Studio 打开本目录（`File → Open`，选 `CameraReceiverPublic` 文件夹）；
2. 等待 Gradle 同步完成（会自动下载 Gradle 8.2 与 AGP 依赖，首次较慢）；
3. `Run ▶` 到手机（或真机 USB 调试）。

命令行方式（Windows，需已装 **JDK 17+** 且配置好 Android SDK）：
```bat
gradlew.bat assembleDebug
```
APK 输出到 `app\build\outputs\apk\debug\app-debug.apk`。

> 注：本机若未安装 JDK/Android SDK，无法直接命令行构建，请使用 Android Studio（自带 JBR JDK）。

## 使用

1. 打开 App → 连接设置页；
2. 输入公网 WebSocket 地址（默认已预填 `ws://214006ed.r27.cpolar.top`）；
3. 点「连接并查看」→ 进入画面显示页，出现实时画面；右上角显示 FPS；
4. 需要改地址时返回设置页修改即可。

## 联调验证

前置条件：
- Windows 端已启动 `relay_server`（监听 UDP 8888 收开发板 + WS 8080 供 cpolar 指向）；
- cpolar HTTP 隧道指向 `localhost:8080`；
- 开发板发送目标 IP 已改为 Windows 主机局域网 IP。

验证步骤：
1. 启动 relay_server，确认其日志显示收到 UDP 分片（`速率` 非 0）；
2. 手机安装本 App，连接公网地址；
3. 若长时间无画面：检查 cpolar 隧道是否指向 8080、relay_server 是否在收包、公网带宽是否足够。

## 常见问题

| 现象 | 排查 |
|---|---|
| 真机连不上（Python 端正常） | ① 检查 app 是否重新安装（需含 `usesCleartextTraffic` 修复）；② 状态栏显示"明文流量被系统拦截"即此问题；③ 确认输入地址含 `ws://` 前缀 |
| 收不到画面 | ① relay_server 是否在收 UDP；② cpolar 隧道是否指向 8080；③ 地址是否正确（ws:// 前缀） |
| 画面卡顿/跳帧 | 协议无重传，丢包即丢帧；免费版 cpolar 带宽有限，可降低开发板 JPEG 质量或帧率 |
| 反复重连 | 公网地址不可达或隧道未生效；用浏览器访问 `http://214006ed.r27.cpolar.top` 确认隧道在线 |
| FPS 低 | 瓶颈在 cpolar 公网带宽与开发板软件编码（q55），非手机解码 |

## 技术说明

- **传输层**：UDP → WebSocket。中继只做校验与字节级转发，每个 WS 二进制消息 = 一个原始
  1324B 分片，故组帧逻辑与原 UDP 场景完全一致。
- **线程模型**：WS 接收（OkHttp 回调线程）→ 帧队列（容量 2，满丢旧帧）→ 解码线程
  （BitmapFactory）→ UI Handler 刷新；接收不被解码拖慢。
- **重连**：指数退避 1s→2s→…→30s 封顶，避免重连风暴；断开即清空组帧状态。
