# ARM 行车记录仪

这是一个运行在 ARM Linux 开发板上的触摸式行车记录仪应用。项目直接使用 Linux framebuffer、Linux input event 和 V4L2 接口，不依赖桌面窗口系统；界面资源以 JPEG/PNG 图片形式放在 `resource/` 目录中。

当前实现覆盖以下功能：

- 800×480 LCD framebuffer 显示与开机动画
- 触摸屏点击及原始坐标到 LCD 坐标的转换
- 通过 V4L2 摄像头采集 640×480 YUYV 图像
- YUYV 转 RGB，并在实时预览中叠加倒车辅助线
- 开始/停止录像、拍照、录像指示灯
- 最近一次录像会话的帧回放
- 三页 framebuffer 可用时的后台绘制和翻页显示

## 硬件与运行环境

程序按当前源码中的固定配置运行，启动前需要确认开发板满足以下条件：

| 项目 | 要求 |
| --- | --- |
| CPU | ARM，能够运行 ARM EABI 32 位程序 |
| 操作系统 | Linux，提供 framebuffer、input 和 V4L2 设备接口 |
| LCD | `/dev/fb0`，800×480，32 bpp，行跨度为 800×4 字节 |
| 触摸屏 | `/dev/input/event0`，原始坐标范围按 1024×600 处理 |
| 摄像头 | `/dev/video7`，支持 V4L2 MMAP 和 YUYV 640×480 |
| 文件系统 | `/root/workspace` 可写，且能创建录像目录和 `photos/` |
| 运行目录 | 必须是包含 `resource/` 的项目目录，当前资源路径是相对路径 `./resource` |

LCD、触摸屏和摄像头设备节点在 `include/app_config.h` 以及 `src/ui_flow.c` 中配置。若开发板设备节点不同，需要修改源码后重新编译。

## 构建

项目使用 CMake，交叉编译器名称默认是 `arm-linux-gcc`，JPEG 库已经随项目放在 `lib/` 目录中。

```sh
cmake -S . -B build-arm -DCMAKE_C_COMPILER=arm-linux-gcc
cmake --build build-arm -j
```

生成文件为：

```text
build-arm/myapp
```

CMake 配置会编译 `src/` 下的模块，并链接 `lib/libjpeg.so`、pthread 和 JPEG 相关头文件。交叉编译环境中至少需要安装：

- CMake 3.13 或更高版本（`CMakeLists.txt` 声明的最低版本为 3.5，但当前构建配置使用了较新的 CMake 命令）
- ARM GCC 交叉编译器，命令名为 `arm-linux-gcc`，或在构建命令中替换为实际编译器名称
- GNU make

仓库中还保留了 `64bit.make.sh`。该脚本包含固定的开发板 IP 地址和远程路径，只适合原始开发环境，不建议直接用于其他板卡。

## 部署与启动

将可执行文件和资源目录复制到开发板，例如：

```sh
scp build-arm/myapp resource root@<开发板IP>:/root/workspace/
```

登录开发板后，从资源目录所在位置启动：

```sh
cd /root/workspace
./myapp
```

程序启动顺序如下：

1. 初始化录像后台线程。
2. 打开并检查 `/dev/fb0`。
3. 打开 `/dev/input/event0`。
4. 播放 `resource/logo/` 下的开机动画，并显示主界面。
5. 进入触摸驱动的 UI 主循环。

程序退出或初始化失败时会释放 framebuffer 映射、触摸设备、摄像头缓冲区和录像编码线程。

## 操作说明

### 主界面

点击主界面中间偏左的实时预览入口即可进入实时画面。当前入口判定范围约为 LCD 坐标 `x=45..159`、`y=160..304`。

### 实时画面

实时画面中央显示 640×480 摄像头图像，两侧显示控制按钮：

| 位置 | 功能 | 当前行为 |
| --- | --- | --- |
| 左上 | 退出实时画面 | 返回主界面，并停止当前录像会话 |
| 左中 | 文件管理 | 当前没有绑定处理逻辑 |
| 左下 | 倒车辅助线 | 切换辅助线显示/隐藏，默认显示 |
| 右上 | 录像 | 开始或停止当前录像会话 |
| 右中 | 查看录像 | 停止实时采集并回放最近录像会话 |
| 右下 | 拍照 | 保存当前帧为一张照片 |

录像时，右上角显示 `REC` 和闪烁红点。实时画面退出后，正在写入的后台任务会在受控流程中停止并回收。

### 录像回放

点击“查看录像”后，程序会在录像根目录中查找名称最新的时间目录，按 `frame` 后的数字顺序播放 JPEG 帧，播放速度约为 33 FPS。回放界面只提供左上角退出按钮；退出后返回主界面。

## 文件保存格式

默认配置如下：

```text
/root/workspace/
├── YYYY-MM-DD_HH-MM-SS/
│   ├── frame0.jpg
│   ├── frame1.jpg
│   └── ...
└── photos/
    ├── photo_YYYY-MM-DD_HH-MM-SS_000.jpg
    └── ...
```

- 录像会话目录名使用本地时间，例如 `2026-07-24_20-00-00`。
- 录像帧从 `frame0.jpg` 开始编号，编号范围为 `0` 到 `199`，超过上限后循环使用编号。
- 录像 JPEG 质量为 75，照片 JPEG 质量为 90。
- 录像编码在独立线程中异步执行，队列深度为 5；队列满时当前帧会被丢弃，可通过 `recorder_dropped_frames()` 查询丢帧数。
- 拍照使用独立缓冲，并优先于普通录像帧编码。
- 回放最多加载 `JPEG_NUM`（当前为 200）帧，并只读取符合 `frame<number>.jpg` 格式的文件。

录像根目录、照片目录、帧尺寸和帧数上限可在 `include/app_config.h` 中调整。

## 项目结构

```text
.
├── CMakeLists.txt          # CMake 构建配置
├── 64bit.make.sh           # 原开发环境中的构建和 SCP 脚本
├── include/                # 公共头文件及应用配置
├── src/
│   ├── main.c              # 设备初始化、资源释放和程序入口
│   ├── ui_flow.c           # 主界面、触摸线程、摄像头线程和回放流程
│   ├── camera.c            # V4L2 摄像头及 MMAP 缓冲区
│   ├── lcd.c               # framebuffer 映射和页面翻转
│   ├── touch.c             # Linux input 触摸事件处理
│   ├── image_convert.c     # YUYV 到 RGB 转换
│   ├── jpeg_codec.c        # JPEG 编码和解码
│   ├── recorder.c           # 异步录像/拍照编码队列
│   ├── playback.c           # 最新录像会话和帧排序
│   ├── live_controls.c      # 实时界面按钮资源和点击区域
│   ├── overlay.c            # 倒车辅助线
│   └── record_indicator.c   # REC 图标和录像红点
├── resource/               # 开机动画、主界面和实时控制按钮图片
├── lib/                    # ARM 版本 libjpeg 动态库及静态库
├── tests/                  # 模块级测试源码
└── build/                  # CMake 构建产物，不建议作为源码依赖
```

模块间的主要数据流为：

```text
V4L2 YUYV 帧
    -> image_convert.c
    -> LCD framebuffer 实时显示
    -> recorder.c 异步复制到队列
    -> jpeg_codec.c 写入 JPEG
```

UI 使用触摸线程接收点击，摄像头线程负责采集、显示、录像和拍照；`UiState`、互斥锁、信号量以及 framebuffer 页面翻转用于协调多线程访问。

## 测试与验证

`tests/` 中包含图像转换、LCD 页面计算、按钮命中、辅助线、回放排序、录像队列和公共头文件等测试源码。当前 CMake 文件只定义主程序目标，没有自动注册测试目标；测试需要在目标架构或对应的交叉编译环境中单独编译链接。

不连接真实设备时无法完成完整运行验证，因为程序依赖 `/dev/fb0`、`/dev/input/event0` 和 `/dev/video7`。部署到开发板后，建议按以下顺序检查：

```sh
ls -l /dev/fb0 /dev/input/event0 /dev/video7
test -r resource/logo/mian_UI.jpeg
test -w /root/workspace
./myapp
```

## 已知限制

- 设备节点、屏幕尺寸、摄像头尺寸和存储路径主要是编译期常量，尚未提供命令行或配置文件覆盖。
- 文件管理按钮目前是预留入口，不会打开文件管理界面。
- `wifi.png`、`settings.png`、`audio_recording.png`、`lock_file.png` 等资源目前未接入主 UI 流程。
- 程序依赖从包含 `resource/` 的目录启动；直接从其他目录运行会导致资源加载失败。
- 摄像头必须接受源码请求的 YUYV 640×480 格式，否则实时线程会退出并返回主界面。
- 录像采用 JPEG 序列而不是视频容器，文件数量和存储占用随录像时间增长，并受 200 个帧编号限制。

## 许可证

项目当前未提供单独的许可证文件。`libjpeg` 相关源码/头文件属于其原有项目，请在再分发前分别确认本项目代码和 libjpeg 的许可证要求。
