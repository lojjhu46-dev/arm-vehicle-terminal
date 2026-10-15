package com.example.camerareceiverpublic;

import android.os.Handler;
import android.os.Looper;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;
import okio.ByteString;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * 公网 WebSocket 接收器：连接 cpolar 隧道，逐包接收中继转发的原协议分片，
 * 校验 -> 组帧 -> 完整 JPEG 帧通过 {@link FrameListener#onFrame} 回调。
 *
 * 每个 WS 二进制消息 = 一个原始 1324B 分片（中继字节级透传）。
 * 断线自动指数退避重连（1s 起，上限 30s），重连期间回调 onStatus。
 */
public class WsReceiver {

    /** 帧队列容量：接收快于解码时丢弃旧帧，保证实时性（与原 app 一致） */
    public static final int QUEUE_CAPACITY = 2;

    /** 重连退避：初始 1s，上限 30s */
    private static final long RECONNECT_BASE_MS = 1000L;
    private static final long RECONNECT_MAX_MS = 30000L;

    public interface FrameListener {
        /** 收到一个完整 JPEG 帧（调用方自行入队/显示） */
        void onFrame(byte[] jpeg);

        /** 连接状态变化：连接中/已连接/已断开/重连/错误 */
        void onStatus(String message);
    }

    private final String url;
    private final FrameListener listener;
    private final OkHttpClient client;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final FrameAssembler assembler = new FrameAssembler();
    private final AtomicBoolean running = new AtomicBoolean(false);
    private final AtomicBoolean reconnectScheduled = new AtomicBoolean(false);

    private long backoffMs = RECONNECT_BASE_MS;
    private WebSocket webSocket;

    public WsReceiver(String url, FrameListener listener) {
        this.url = url;
        this.listener = listener;
        this.client = new OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(0, TimeUnit.MILLISECONDS) // 长连接，不设读超时
                .pingInterval(20, TimeUnit.SECONDS)
                .build();
    }

    /** 建立 WS 连接并启动重连循环 */
    public void start() {
        running.set(true);
        connect();
    }

    /** 关闭连接并停止重连 */
    public void stop() {
        running.set(false);
        WebSocket ws = webSocket;
        if (ws != null) {
            ws.close(1000, "stop");
            webSocket = null;
        }
    }

    private void connect() {
        if (!running.get()) return;
        notifyStatus("连接中…");
        Request request = new Request.Builder().url(url).build();
        webSocket = client.newWebSocket(request, new WebSocketListener() {
            @Override
            public void onOpen(WebSocket ws, Response response) {
                backoffMs = RECONNECT_BASE_MS;
                notifyStatus("已连接");
            }

            @Override
            public void onMessage(WebSocket ws, ByteString bytes) {
                handleMessage(bytes.toByteArray());
            }

            @Override
            public void onMessage(WebSocket ws, String text) {
                // 非预期文本消息，忽略
            }

            @Override
            public void onClosed(WebSocket ws, int code, String reason) {
                scheduleReconnect();
            }

            @Override
            public void onFailure(WebSocket ws, Throwable t, Response response) {
                String detail = t != null ? String.valueOf(t.getMessage()) : "未知错误";
                if (response != null) {
                    detail += " (HTTP " + response.code() + ")";
                } else if (t != null && t.getMessage() != null
                        && t.getMessage().toLowerCase().contains("cleartext")) {
                    detail = "明文流量被系统拦截，请检查 usesCleartextTraffic 配置";
                }
                notifyStatus("连接异常: " + detail);
                scheduleReconnect();
            }
        });
    }

    private void handleMessage(byte[] raw) {
        if (!running.get()) return;
        // 长度必须与协议结构一致
        if (raw.length != CameraPacket.TOTAL_SIZE) {
            assembler.reset();
            return;
        }
        CameraPacket packet = CameraPacket.parse(raw);
        if (!packet.isValid()) {
            assembler.reset();
            return;
        }
        byte[] frame = assembler.feed(packet);
        if (frame != null) {
            listener.onFrame(frame);
        }
    }

    private void scheduleReconnect() {
        if (!running.get()) return;
        if (!reconnectScheduled.compareAndSet(false, true)) return;
        long delay = backoffMs;
        backoffMs = Math.min(backoffMs * 2, RECONNECT_MAX_MS);
        notifyStatus("已断开，将在 " + (delay / 1000) + "s 后重连…");
        mainHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                reconnectScheduled.set(false);
                assembler.reset(); // 断线后清空组帧状态，避免跨连接拼接半帧
                connect();
            }
        }, delay);
    }

    private void notifyStatus(final String message) {
        if (listener == null) return;
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                listener.onStatus(message);
            }
        });
    }
}
