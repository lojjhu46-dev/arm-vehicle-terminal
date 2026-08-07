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
- 主界面底图上的 FreeType 时间、日期、天气和温度叠加
- 12 种居中天气图标及线程安全的动态天气状态接口
- 主界面时间按分钟自动刷新，天气状态变化后自动刷新
- 只在文件管理入口启动的 LVGL 文件浏览器
- 会员扩展页、图片登录页和会员中心页的页面跳转
- 登录页手机号/短信验证码数字键盘输入与 FreeType 数字叠加
- 后台短信验证码请求、验证码校验和登录失败提示
- 三个会员页面左上角返回按钮及逐级返回主界面

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
| 字库 | FreeType ARM SDK，默认路径为 `../freetype/freetype_arm` |

LCD、触摸屏和摄像头设备节点在 `include/app_config.h` 以及 `src/ui_flow.c` 中配置。若开发板设备节点不同，需要修改源码后重新编译。

## 构建

项目使用 CMake，交叉编译器名称默认是 `arm-linux-gcc`，JPEG 库已经随项目放在 `lib/` 目录中。LVGL 和 FreeType 路径可以通过 CMake 变量覆盖。

```sh
cmake -S . -B build-lvgl-arm \
  -DCMAKE_C_COMPILER=arm-linux-gcc \
  -DLVGL_ROOT_DIR=/home/gec/workspace/lvgl_project/lv_port_pc_vscode/lvgl \
  -DFREETYPE_ROOT_DIR=/home/gec/workspace/freetype/freetype_arm
cmake --build build-lvgl-arm --parallel 2
```

生成文件为：

```text
build-lvgl-arm/myapp
```

CMake 配置会编译 `src/` 下的模块和 LVGL，并链接 FreeType、JPEG、pthread、数学库和动态加载库。交叉编译环境中至少需要安装：

- CMake 3.13 或更高版本（`CMakeLists.txt` 声明的最低版本为 3.5，但当前构建配置使用了较新的 CMake 命令）
- ARM GCC 交叉编译器，命令名为 `arm-linux-gcc`，或在构建命令中替换为实际编译器名称
- GNU make
- LVGL 9.5 源码目录；可通过 `LVGL_ROOT_DIR` 指向本机的 LVGL checkout
- FreeType ARM SDK；可通过 `FREETYPE_ROOT_DIR` 指向本机的 FreeType SDK
- ImageMagick 的 `convert` 命令，仅在重新生成天气资源时需要

也可以使用项目根目录下的 `64bit.make.sh` 一键生成天气资源并完成 ARM 构建。脚本默认只构建，不执行远程上传：

```sh
./64bit.make.sh
```

需要清理构建目录时显式设置 `CLEAN_BUILD=1`；需要执行原有的 SCP 部署流程时设置 `DEPLOY=1`，并可用 `BOARD_APP_ONLY`、`BOARD_FULL` 和 `REMOTE_ROOT` 覆盖目标。

## 部署与启动

将可执行文件和资源目录复制到开发板，例如：

```sh
scp build-lvgl-arm/myapp resource root@<开发板IP>:/root/workspace/
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

主界面使用 `resource/logo/mian_UI.jpeg` 作为底图。时间、日期、城市、温度和天气文字由 FreeType 在运行时绘制，天气图标使用 `resource/Weather/` 中与底图背景匹配的 `96×64` JPEG，显示位置为 `x=352,y=68`。

UI 主循环每 `250ms` 检查一次，但只有跨分钟或天气版本变化时才重绘主界面，因此时间显示按分钟更新。天气默认值为深圳 `23℃`、晴转多云；其他模块可通过 `include/main_weather.h` 中的 `main_weather_set()` 更新城市、温度和天气类型。

### 会员扩展与登录

主界面右下角的“更多应用”入口约命中 LCD 坐标 `x=635..769`、`y=325..464`，点击后直接刷出 `resource/expand.jpg`。扩展页底部中央的“去登录”按钮进入 `resource/Menberslogin.jpg`；登录页的“登录”按钮只有在验证码校验成功后才会进入 `resource/MembersSanctuary.jpg`。

三个图片页面都使用 framebuffer 刷图，不使用 LVGL。左上角的 `resource/back.jpg`（由提供的 `back.png` 转换而来）按以下顺序返回：会员中心 → 登录页 → 扩展页 → 主界面。

登录页使用 `src/member_login.c` 维护输入状态，数字键盘布局与图片一致：手机号限定 11 位，短信验证码限定 4 位；点击手机号或验证码输入框切换焦点，左下角按键退格，右下角按键执行登录校验。已输入的手机号和验证码会通过 FreeType 在对应输入栏显示，发送中、发送失败、验证码错误等状态显示在登录按钮下方。

点击“获取验证码”后，`src/member_sms.c` 在后台线程调用参考登录工程使用的互亿无线 HTTP 接口（`api.ihuyi.com:80`）。服务商账号和密码当前以 C 源码常量配置在该文件中；部署前应轮换为自己的凭据。网络请求不会阻塞触摸读取线程，但需要开发板具备 DNS 和外网访问能力。登录态仅保存在当前进程内，程序重启后不会自动恢复。

### 实时画面

实时画面中央显示 640×480 摄像头图像，两侧显示控制按钮：

| 位置 | 功能 | 当前行为 |
| --- | --- | --- |
| 左上 | 退出实时画面 | 返回主界面，并停止当前录像会话 |
| 左中 | 文件管理 | 停止实时采集后打开 LVGL 文件浏览器 |
| 左下 | 倒车辅助线 | 切换辅助线显示/隐藏，默认显示 |
| 右上 | 录像 | 开始或停止当前录像会话 |
| 右中 | 查看录像 | 停止实时采集并回放最近录像会话 |
| 右下 | 拍照 | 保存当前帧为一张照片 |

录像时，右上角显示 `REC` 和闪烁红点。实时画面退出后，正在写入的后台任务会在受控流程中停止并回收。

### 录像回放

点击“查看录像”后，程序会在 `recordings/` 目录中查找名称最新的时间目录，按 `frame` 后的数字顺序播放 JPEG 帧，播放速度约为 33 FPS。回放界面只提供左上角退出按钮；退出后返回主界面。

文件浏览器固定在 `/root/workspace` 内浏览，忽略符号链接，并拒绝根目录外的路径。点击 `photos/` 中的 `.jpg` 或 `.jpeg` 文件会显示照片；点击名称符合录像会话格式且包含 `frame<number>.jpg` 的目录，会关闭浏览器并进入原有回放流程。

## 文件保存格式

默认配置如下：

```text
/root/workspace/
├── recordings/
│   └── YYYY-MM-DD_HH-MM-SS/
│       ├── frame0.jpg
│       ├── frame1.jpg
│       └── ...
└── photos/
    ├── photo_YYYY-MM-DD_HH-MM-SS_000.jpg
    └── ...
```

- 新录像会话保存在 `recordings/`，旧版直接位于 `/root/workspace` 的会话目录不会移动或改写。
- 录像会话目录名使用本地时间，例如 `2026-07-24_20-00-00`。
- 录像帧从 `frame0.jpg` 开始编号，编号范围为 `0` 到 `199`，超过上限后循环使用编号。
- 录像 JPEG 质量为 75，照片 JPEG 质量为 90。
- 录像编码在独立线程中异步执行，队列深度为 5；队列满时当前帧会被丢弃，可通过 `recorder_dropped_frames()` 查询丢帧数。
- 拍照使用独立缓冲，并优先于普通录像帧编码。
- 回放最多加载 `JPEG_NUM`（当前为 200）帧，并只读取符合 `frame<number>.jpg` 格式的文件。

录像根目录、照片目录、帧尺寸和帧数上限可在 `include/app_config.h` 中调整。

## 天气资源与和风天气 API

天气图集源文件为 `resource/Weather/weather.png`，可通过以下脚本重新生成 12 张主界面天气资源：

```sh
./scripts/generate_weather_assets.sh
```

脚本会逐格裁剪图标、去除棋盘背景、统一居中缩放，并使用主界面顶部背景导出 JPEG。原始图集不会被修改。

当前固件只提供本地的 `main_weather_set()` 接口，尚未直接联网请求和风天气。根据官方文档，后续应使用当前推荐的小时天气预报 v1 接口：

```text
GET /weather/v1/hourly/{latitude}/{longitude}?hours=24&localTime=true&lang=zh
```

请求必须使用 HTTPS、控制台分配的专属 API Host，以及 JWT `Authorization: Bearer <token>` 或 API Key 鉴权。返回数据中的 `hours[]` 提供 `condition.text`、`condition.code` 和 `temperature.value` 等字段，可映射到 `main_weather_set()`。旧版 `/v7/weather/{hours}` 城市小时预报已标记为即将弃用，不应作为新实现的接口。

当前工程不直接请求和风天气。外部客户端负责 TLS、DNS、鉴权和 JSON 响应处理，再将固定格式的 24 小时预报经 TCP 下发到板端；板端只负责校验、缓存和显示。详细接口以[和风天气小时天气预报文档](https://dev.qweather.com/docs/api/weather/weather-hourly-forecast/)为准。

### TCP 天气接收

程序启动后会在 TCP `0.0.0.0:8080` 监听。客户端发送 UTF-8 文本后必须执行 `shutdown(SHUT_WR)`；服务端读取到 EOF 才处理该请求，回复文本并关闭连接。

以 `{` 开头的数据按 `weather.hourly.v1` 解析，要求包含城市、坐标和恰好 24 条 `time`、`text`、`temp_c` 小时数据。预报缓存在内存中，收到后立即使用当前 `+08:00` 小时的数据；如果当前小时不在缓存中，则使用最近未来小时，全部过期时使用最新一条；每到下一个整点再从缓存中切换主界面天气图标、文字和温度。服务端会将收到的 JSON、解析结果和状态栏更新结果打印到终端。协议外的 JSON、超出 8KB 的请求和无效字段会收到 `ERROR` 回复。

不以 `{` 开头的数据视为普通 AI 文本。固件识别以下五条 UTF-8 文本指令及包含对应动作短语的 AI 回复：`开始录像`、`停止录像`、`拍照`、`开启倒车线`、`关闭倒车线`。带空格或换行的孤立指令仍不会匹配；识别成功后回复 `OK command accepted`，其余文本仍会记录并回复 `OK text received`。录像、拍照和倒车线操作由 UI/摄像头线程执行，不在 TCP 接收线程直接访问设备。

## 项目结构

```text
.
├── CMakeLists.txt          # CMake 构建配置
├── 64bit.make.sh           # 天气资源生成、ARM 构建和可选 SCP 部署
├── include/                # 公共头文件及应用配置
├── src/
│   ├── main.c              # 设备初始化、资源释放和程序入口
│   ├── ui_flow.c           # 主界面、触摸线程、摄像头线程和回放流程
│   ├── member_login.c      # 会员登录输入、验证码状态和校验逻辑
│   ├── member_sms.c        # 短信服务请求及后台发送线程
│   ├── main_weather.c      # 天气资源缓存、状态接口和图标绘制
│   ├── weather_receiver.c  # TCP 文本/24小时天气接收、缓存和整点切换
│   ├── ui_remote_commands.c # 远程文本指令邮箱及精确匹配处理器
│   ├── freetype_lcd.c      # FreeType LCD 文字绘制封装
│   ├── camera.c            # V4L2 摄像头及 MMAP 缓冲区
│   ├── lcd.c               # framebuffer 映射和页面翻转
│   ├── touch.c             # Linux input 触摸事件处理
│   ├── image_convert.c     # YUYV 到 RGB 转换
│   ├── jpeg_codec.c        # JPEG 编码和解码
│   ├── recorder.c           # 异步录像/拍照编码队列
│   ├── playback.c           # 最新录像会话和帧排序
│   ├── live_controls.c      # 实时界面按钮资源和点击区域
│   ├── lvgl_file_browser.c  # 独立的 LVGL 文件浏览器及 LCD/触摸适配器
│   ├── overlay.c            # 倒车辅助线
│   └── record_indicator.c   # REC 图标和录像红点
├── resource/               # 开机动画、主界面和实时控制按钮图片
│   ├── Weather/             # 12 张主界面天气 JPEG 和原始天气图集
│   ├── expand.jpg           # 会员扩展页
│   ├── Menberslogin.jpg     # 会员登录页（文件名按现有资源保留）
│   ├── MembersSanctuary.jpg # 会员中心页
│   ├── back.jpg             # 图片页返回按钮
│   └── fonts/               # SIMHEI.TTF 字库
├── scripts/
│   └── generate_weather_assets.sh # 天气图标裁剪和背景合成脚本
├── lib/                    # ARM 版本 libjpeg 动态库及静态库
├── tests/                  # 模块级测试源码
└── build-lvgl-arm/         # LVGL/FreeType ARM 构建产物
```

`lv_conf.h` 只启用浏览器所需的 LVGL 软件绘制、对象、按钮、标签和图片，并关闭 Linux framebuffer、evdev 及 LVGL 文件式 JPEG 解码。浏览器复用已有 `Lcd` 和 `Touch` 对象，不会再次打开 `/dev/fb0` 或 `/dev/input/event0`。会员扩展和登录页面不使用 LVGL，只在已有 framebuffer 上刷 JPEG 并叠加 FreeType 文本。

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

`tests/` 中包含图像转换、LCD 页面计算、按钮命中、辅助线、回放排序、录像队列、公共头文件和会员登录状态测试源码。`test_member_login` 只在 CMake 检测到的编译器名称不包含 `arm-linux` 时注册为 CTest 测试；ARM 交叉构建不会尝试在宿主机执行 ARM 测试二进制。

在具备项目依赖的宿主构建中可运行：

```sh
cmake -S . -B build-host \
  -DCMAKE_C_COMPILER=gcc \
  -DLVGL_ROOT_DIR=/path/to/lvgl \
  -DFREETYPE_ROOT_DIR=/path/to/freetype_arm
cmake --build build-host --target test_member_login
ctest --test-dir build-host --output-on-failure
```

会员登录测试不访问真实短信服务；它直接模拟短信完成结果以验证输入和状态机。真实短信请求只能在开发板或具备网络的运行环境中验证。

不连接真实设备时无法完成完整运行验证，因为程序依赖 `/dev/fb0`、`/dev/input/event0` 和 `/dev/video7`。部署到开发板后，建议按以下顺序检查：

```sh
ls -l /dev/fb0 /dev/input/event0 /dev/video7
test -r resource/logo/mian_UI.jpeg
test -w /root/workspace
./myapp
```

## 已知限制

- 设备节点、屏幕尺寸、摄像头尺寸和存储路径主要是编译期常量，尚未提供命令行或配置文件覆盖。
- 文件浏览器不提供删除、重命名、搜索或 PNG 解码。
- 会员登录不保存登录态，不实现账号数据库、注册页、短信倒计时或发送频率限制。
- 短信接口当前使用 HTTP 明文传输，服务商凭据位于 `src/member_sms.c`，不应直接复用到生产环境。
- 和风天气 API 尚未接入，当前天气数据不会自动从网络更新。
- `wifi.png`、`settings.png`、`audio_recording.png`、`lock_file.png` 等资源目前未接入主 UI 流程。
- 程序依赖从包含 `resource/` 的目录启动；直接从其他目录运行会导致资源加载失败。
- 摄像头必须接受源码请求的 YUYV 640×480 格式，否则实时线程会退出并返回主界面。
- 录像采用 JPEG 序列而不是视频容器，文件数量和存储占用随录像时间增长，并受 200 个帧编号限制。

## 许可证

项目当前未提供单独的许可证文件。`libjpeg` 相关源码/头文件属于其原有项目，请在再分发前分别确认本项目代码和 libjpeg 的许可证要求。
