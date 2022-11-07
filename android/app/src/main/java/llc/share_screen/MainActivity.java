package llc.share_screen;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;
import android.text.Layout;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

/** main ui. */
public class MainActivity extends AppCompatActivity {
    public static final String TAG = "my.MainActivity";

    /** update log widgets. */
    public void updateLogUi() {
        TextView label_log = findViewById(R.id.label_log);
        label_log.setText(FrontThread.GetSingleton().getLogString());
        Layout layout = label_log.getLayout();
        if (layout != null) {
            int scroll = layout.getLineTop(label_log.getLineCount()) - label_log.getHeight();
            label_log.scrollTo(0, Math.max(scroll, 0));
        }
    }

    /** update state widgets. */
    public void updateUi() {
        boolean isClosed = FrontThread.GetSingleton().isClosed();
        Button but_start = findViewById(R.id.but_start);
        Button but_stop = findViewById(R.id.but_stop);
        but_start.setEnabled(isClosed);
        but_stop.setEnabled(!isClosed);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "onCreate");

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        ActivityResultLauncher<Intent> resultLauncher = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(), (ActivityResult result) -> {
                if (result.getResultCode() == Activity.RESULT_OK) {
                    Intent serviceIntent = new Intent(this, MainService.class);
                    serviceIntent.putExtra(Intent.EXTRA_INTENT, result.getData());
                    startForegroundService(serviceIntent);
                }
            });

        TextView label_log = findViewById(R.id.label_log);
        label_log.setMovementMethod(new ScrollingMovementMethod());

        Button but_setting = findViewById(R.id.but_setting);
        but_setting.setOnClickListener((View v) -> {
            EditText ed = new EditText(this);
            ed.setText(FrontThread.GetSingleton().readSetting());
            ed.requestFocus();
            AlertDialog dialog = new AlertDialog.Builder(this)
                                     .setView(ed)
                                     .setPositiveButton(R.string.ok, null)
                                     .setNegativeButton(R.string.cancel, null)
                                     .create();
            dialog.getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
            dialog.show();
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener((View vv) -> {
                FrontThread.GetSingleton().writeSetting(ed.getText().toString());
                dialog.dismiss();
            });
        });

        Button but_copy_log = findViewById(R.id.but_copy_log);
        but_copy_log.setOnClickListener((View v) -> {
            ClipboardManager clipboardMgr =
                (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData p = ClipData.newPlainText("log", FrontThread.GetSingleton().getLogString());
            clipboardMgr.setPrimaryClip(p);
        });

        Button but_start = findViewById(R.id.but_start);
        but_start.setOnClickListener((View v) -> {
            v.setEnabled(false);
            MediaProjectionManager pMgr =
                (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            resultLauncher.launch(pMgr.createScreenCaptureIntent());
        });

        Button but_stop = findViewById(R.id.but_stop);
        but_stop.setOnClickListener((View v) -> {
            v.setEnabled(false);
            FrontThread.GetSingleton().opStop();
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume");
        FrontThread.GetSingleton().setMainActivity(this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause");
        FrontThread.GetSingleton().setMainActivity(null);
    }
}