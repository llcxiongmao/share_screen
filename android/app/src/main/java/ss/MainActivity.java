package ss;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;
import android.text.Layout;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        android.util.Log.i(TAG, "onCreate");

        setContentView(R.layout.activity_main);

        ActivityResultLauncher<Intent> resultLauncher = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), (ActivityResult result) -> {
                if (result.getResultCode() == Activity.RESULT_OK) {
                    Intent serviceIntent = new Intent(this, MainService.class);
                    serviceIntent.putExtra(Intent.EXTRA_INTENT, result.getData());
                    startForegroundService(serviceIntent);
                } else {
                    MainThread.Singleton().notifyLog('E', "screen capture permission denied");
                    MainThread.Singleton().mState = MainThread.STATE_STOPPED;
                    refreshState();
                }
            });

        TextView label_log = findViewById(R.id.label_log);
        label_log.setHorizontallyScrolling(true);
        label_log.setMovementMethod(new ScrollingMovementMethod());

        LinearLayout group_other_setting = findViewById(R.id.group_other_setting);
        group_other_setting.setVisibility(View.GONE);
        Button but_other_setting = findViewById(R.id.but_other_setting);
        but_other_setting.setOnClickListener((View v) -> {
            if (group_other_setting.getVisibility() != View.GONE) {
                group_other_setting.setVisibility(View.GONE);
            } else {
                group_other_setting.setVisibility(View.VISIBLE);
            }
        });

        Button but_copy_log = findViewById(R.id.but_copy_log);
        but_copy_log.setOnClickListener((View v) -> {
            ClipboardManager clipboardMgr =
                    (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData p = ClipData.newPlainText("log", MainThread.Singleton().mLogCache.toString());
            clipboardMgr.setPrimaryClip(p);
        });

        Button but_start = findViewById(R.id.but_start);
        but_start.setOnClickListener((View v) -> {
            MainThread.Singleton().mState = MainThread.STATE_STARTING;
            refreshState();
            syncConfig();
            MainThread.Singleton().mLogCache.setLength(0);
            refreshLog();

            MediaProjectionManager pMgr =
                (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            resultLauncher.launch(pMgr.createScreenCaptureIntent());
        });

        Button but_stop = findViewById(R.id.but_stop);
        but_stop.setOnClickListener((View v) -> {
            MainThread.Singleton().mState = MainThread.STATE_STOPPING;
            refreshState();

            SessionThread.Singleton().notifyClose();
        });

        mSetting = getSharedPreferences("share_screen", MODE_PRIVATE);

        MainThread.Singleton().mMainActivity = this;
        refreshLog();
        refreshConfig();
        refreshState();
    }

    public void refreshLog() {
        TextView label_log = findViewById(R.id.label_log);
        label_log.setText(MainThread.Singleton().mLogCache.toString());
        Layout layout = label_log.getLayout();
        if (layout != null) {
            int scrollY = Math.max(layout.getHeight() - label_log.getHeight(), 0);
            label_log.scrollTo(label_log.getScrollX(), scrollY);
        }
    }

    public void refreshConfig() {
        EditText edit_bit_rate = findViewById(R.id.edit_bit_rate);
        EditText edit_i_interval = findViewById(R.id.edit_i_interval);
        EditText edit_port = findViewById(R.id.edit_port);
        EditText edit_broadcast_port = findViewById(R.id.edit_broadcast_port);
        CheckBox check_debug_encode = findViewById(R.id.check_debug_encode);
        CheckBox check_debug_net = findViewById(R.id.check_debug_net);
        loadIntSetting(edit_bit_rate, Config.Singleton().bit_rate);
        loadIntSetting(edit_i_interval, Config.Singleton().i_interval);
        loadIntSetting(edit_port, Config.Singleton().port);
        loadIntSetting(edit_broadcast_port, Config.Singleton().broadcast_port);
        loadBoolSetting(check_debug_encode, Config.Singleton().debug_encode);
        loadBoolSetting(check_debug_net, Config.Singleton().debug_net);
    }

    public void refreshState() {
        Button but_start = findViewById(R.id.but_start);
        Button but_stop = findViewById(R.id.but_stop);
        switch (MainThread.Singleton().mState) {
            case MainThread.STATE_STOPPED:
                but_start.setEnabled(true);
                but_stop.setEnabled(false);
                break;
            case MainThread.STATE_STARTING:
            case MainThread.STATE_STOPPING:
                but_start.setEnabled(false);
                but_stop.setEnabled(false);
                break;
            case MainThread.STATE_STARTED:
                but_start.setEnabled(false);
                but_stop.setEnabled(true);
                break;
        }
    }

    public void syncConfig() {
        EditText edit_bit_rate = findViewById(R.id.edit_bit_rate);
        EditText edit_i_interval = findViewById(R.id.edit_i_interval);
        EditText edit_port = findViewById(R.id.edit_port);
        EditText edit_broadcast_port = findViewById(R.id.edit_broadcast_port);
        CheckBox check_debug_encode = findViewById(R.id.check_debug_encode);
        CheckBox check_debug_net = findViewById(R.id.check_debug_net);
        storeIntSetting(edit_bit_rate, Config.Singleton().bit_rate);
        storeIntSetting(edit_i_interval, Config.Singleton().i_interval);
        storeIntSetting(edit_port, Config.Singleton().port);
        storeIntSetting(edit_broadcast_port, Config.Singleton().broadcast_port);
        storeBoolSetting(check_debug_encode, Config.Singleton().debug_encode);
        storeBoolSetting(check_debug_net, Config.Singleton().debug_net);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        android.util.Log.i(TAG, "onDestroy");
        MainThread.Singleton().mMainActivity = null;
    }

    @Override
    protected void onResume() {
        super.onResume();
        android.util.Log.i(TAG, "onResume");
    }

    @Override
    protected void onPause() {
        super.onPause();
        android.util.Log.i(TAG, "onPause");
    }

    private void loadIntSetting(EditText edit, Config.IntValue v) {
        v.value = mSetting.getInt(v.name, v.defaultValue);
        if (v.value < v.min || v.value > v.max) {
            SharedPreferences.Editor ed = mSetting.edit();
            ed.putInt(v.name, v.defaultValue);
            ed.apply();
            MainThread.Singleton().notifyLog(
                'W',
                String.format(
                    "invalid setting %s: %d, acceptable range: [%d, %d], adjust to default value: %d",
                    v.name,
                    v.value,
                    v.min,
                    v.max,
                    v.defaultValue));
            v.value = v.defaultValue;
        }
        edit.setText(String.valueOf(v.value));
    }

    private void storeIntSetting(EditText edit, Config.IntValue v) {
        try {
            v.value = Integer.parseInt(edit.getText().toString());
        } catch (Exception e) {
            v.value = v.defaultValue;
        }
        if (v.value < v.min || v.value > v.max) {
            MainThread.Singleton().notifyLog(
                'W',
                String.format(
                    "invalid setting %s: %d, acceptable range: [%d, %d], adjust to default value: %d",
                    v.name,
                    v.value,
                    v.min,
                    v.max,
                    v.defaultValue));
            v.value = v.defaultValue;
        }
        edit.setText(String.valueOf(v.value));
        SharedPreferences.Editor ed = mSetting.edit();
        ed.putInt(v.name, v.value);
        ed.apply();
    }

    private void loadBoolSetting(CheckBox check, Config.BoolValue v) {
        v.value = mSetting.getBoolean(v.name, v.defaultValue);
        check.setChecked(v.value);
    }

    private void storeBoolSetting(CheckBox check, Config.BoolValue v) {
        v.value = check.isChecked();
        SharedPreferences.Editor ed = mSetting.edit();
        ed.putBoolean(v.name, v.value);
        ed.apply();
    }

    public static final String TAG = "ss.MainActivity";

    private SharedPreferences mSetting;
}