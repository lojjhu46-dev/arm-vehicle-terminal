package com.example.camerareceiverpublic;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * 画面显示页：顶部状态栏 + 视频画面 + 右上角 FPS，接收公网 WS 分片并实时显示。
 *
 * 线程模型：
 *  - WsReceiver（OkHttp 回调线程）：收包校验 + 组帧，完整帧入队；
 *  - 解码线程：从队列取帧，BitmapFactory 解码，交给 UI 线程显示；
 *  - 队列容量 2，满则丢旧帧，保证实时性（接收不被解码拖慢）。
 */
public class VideoActivity extends Activity {

    private static final long FPS_REFRESH_INTERVAL_MS = 1000L;

    private ImageView videoView;
    private TextView statusText;
    private TextView fpsText;
    private Button btnRotate;

    private final BlockingQueue<byte[]> frameQueue =
            new ArrayBlockingQueue<>(WsReceiver.QUEUE_CAPACITY);
    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private final AtomicInteger decodedFrames = new AtomicInteger(0);

    private volatile boolean running = false;
    private WsReceiver receiver;
    private Thread decodeThread;

    private long fpsTickStart = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        videoView = findViewById(R.id.video_view);
        statusText = findViewById(R.id.status_text);
        fpsText = findViewById(R.id.fps_text);
        btnRotate = findViewById(R.id.btn_rotate);
        Button btnBack = findViewById(R.id.btn_back);

        btnBack.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                finish();
            }
        });

        // 竖屏/横屏手动切换：点击在 LANDSCAPE / PORTRAIT 间切换
        btnRotate.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int cur = getResources().getConfiguration().orientation;
                setRequestedOrientation(
                        cur == Configuration.ORIENTATION_LANDSCAPE
                                ? ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                                : ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            }
        });

        String url = getIntent().getStringExtra(MainActivity.EXTRA_WS_URL);
        if (url == null) url = MainActivity.DEFAULT_WS_URL;

        // 初始按当前方向应用 UI 状态（横屏进入时立即沉浸）
        applyOrientation(getResources().getConfiguration().orientation);
        start(url);
    }

    /**
     * 方向变化回调（Manifest 中 configChanges 声明 orientation|screenSize，
     * 因此旋转不重建 Activity，WS 连接与解码线程保持不断开）。
     * 顶部栏为悬浮层，横竖屏均始终可见（含旋转/返回按钮）；
     * 横屏额外隐藏系统状态栏/导航栏进入沉浸式全屏，竖屏恢复系统栏。
     */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        applyOrientation(newConfig.orientation);
    }

    private void applyOrientation(int orientation) {
        if (btnRotate != null) {
            btnRotate.setText(orientation == Configuration.ORIENTATION_LANDSCAPE
                    ? R.string.btn_rotate_to_portrait
                    : R.string.btn_rotate_to_landscape);
        }
        View decor = getWindow().getDecorView();
        if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
            // 横屏沉浸式：隐藏系统状态栏/导航栏，画面全屏（顶部悬浮栏仍可见）
            int flags = View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            decor.setSystemUiVisibility(flags);
        } else {
            // 竖屏：恢复系统栏
            decor.setSystemUiVisibility(0);
        }
    }

    private void start(String url) {
        running = true;

        receiver = new WsReceiver(url, new WsReceiver.FrameListener() {
            @Override
            public void onFrame(byte[] jpeg) {
                // 队列满则丢弃旧帧（保证实时性）
                if (frameQueue.remainingCapacity() == 0) {
                    frameQueue.poll();
                }
                frameQueue.offer(jpeg);
            }

            @Override
            public void onStatus(final String message) {
                uiHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        statusText.setText(message);
                        if (message.startsWith("已连接")) {
                            statusText.setTextColor(getResources().getColor(R.color.success));
                        } else if (message.startsWith("连接中")) {
                            statusText.setTextColor(getResources().getColor(R.color.warning));
                        } else {
                            statusText.setTextColor(getResources().getColor(R.color.danger));
                        }
                    }
                });
            }
        });

        decodeThread = new Thread(new Runnable() {
            @Override
            public void run() {
                decodeLoop();
            }
        }, "jpeg-decode");
        decodeThread.start();

        receiver.start();

        uiHandler.postDelayed(fpsRunnable, FPS_REFRESH_INTERVAL_MS);
    }

    private void decodeLoop() {
        while (running) {
            final byte[] jpeg;
            try {
                jpeg = frameQueue.take();
            } catch (InterruptedException e) {
                break;
            }
            final Bitmap bitmap = BitmapFactory.decodeByteArray(jpeg, 0, jpeg.length);
            if (bitmap == null) continue; // 解码失败（理论上组帧已保证完整性）
            uiHandler.post(new Runnable() {
                @Override
                public void run() {
                    videoView.setImageBitmap(bitmap);
                    decodedFrames.incrementAndGet();
                }
            });
        }
    }

    private final Runnable fpsRunnable = new Runnable() {
        @Override
        public void run() {
            long now = System.currentTimeMillis();
            if (fpsTickStart == 0) {
                fpsTickStart = now;
            } else {
                long elapsed = now - fpsTickStart;
                if (elapsed >= FPS_REFRESH_INTERVAL_MS) {
                    int frames = decodedFrames.getAndSet(0);
                    long fps = frames * 1000L / elapsed;
                    fpsText.setText(String.format(getString(R.string.fps_label), fps));
                    fpsTickStart = now;
                }
            }
            if (running) {
                uiHandler.postDelayed(this, FPS_REFRESH_INTERVAL_MS);
            }
        }
    };

    @Override
    protected void onDestroy() {
        super.onDestroy();
        running = false;
        if (receiver != null) receiver.stop();
        if (decodeThread != null) decodeThread.interrupt();
        uiHandler.removeCallbacksAndMessages(null);
    }
}
