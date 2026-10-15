package com.example.camerareceiverpublic;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

/**
 * 连接设置页：输入公网 WebSocket 地址（预填默认值），保存后跳转画面显示页。
 *
 * 地址变更随时可改；SharedPreferences 持久化，下次启动自动填充。
 */
public class MainActivity extends Activity {

    public static final String EXTRA_WS_URL = "ws_url";
    public static final String DEFAULT_WS_URL = "ws://214006ed.r27.cpolar.top";
    private static final String PREFS_NAME = "cam_prefs";
    private static final String KEY_WS_URL = "ws_url";

    private EditText etAddress;
    private TextView tvStatus;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        etAddress = findViewById(R.id.et_address);
        tvStatus = findViewById(R.id.tv_status);
        Button btnConnect = findViewById(R.id.btn_connect);

        // 恢复上次保存的地址；无则用默认值
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String saved = prefs.getString(KEY_WS_URL, DEFAULT_WS_URL);
        etAddress.setText(saved);
        if (!TextUtils.isEmpty(saved)) {
            etAddress.setSelection(saved.length());
        }

        btnConnect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String url = etAddress.getText().toString().trim();
                if (TextUtils.isEmpty(url)) {
                    Toast.makeText(MainActivity.this, "请输入公网 WebSocket 地址",
                            Toast.LENGTH_SHORT).show();
                    return;
                }
                // 规范化：补全 ws:// 前缀
                if (!url.startsWith("ws://") && !url.startsWith("wss://")) {
                    url = "ws://" + url;
                }
                prefs.edit().putString(KEY_WS_URL, url).apply();

                tvStatus.setText("正在连接…");
                Intent intent = new Intent(MainActivity.this, VideoActivity.class);
                intent.putExtra(EXTRA_WS_URL, url);
                startActivity(intent);
            }
        });
    }
}
