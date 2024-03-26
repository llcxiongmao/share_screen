package ss;

import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import androidx.annotation.NonNull;

public class MainThread extends Application {
    public static MainThread Singleton() {
        return sSingleton;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        android.util.Log.i(TAG, "onCreate");
        sSingleton = this;

        new Config();

        mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(@NonNull Message msg) {
                if (msg.what == MSG_LOG) {
                    mLogCache.append("[");
                    mLogCache.append((char) msg.arg1);
                    mLogCache.append("] ");
                    mLogCache.append((String) msg.obj);
                    mLogCache.append("\n");
                    if (mMainActivity != null) {
                        mMainActivity.refreshLog();
                    }
                } else if (msg.what == MSG_CONTROL_THREAD_EXITED) {
                    stopService(new Intent(MainThread.this, MainService.class));
                    SessionThread.SetSingleton(null);
                    mState = STATE_STOPPED;
                    if (mMainActivity != null) {
                        mMainActivity.refreshState();
                    }
                }
            }
        };

        mLogCache = new StringBuilder();

        NotificationManager notifyMgr =
            (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        NotificationChannel notifyChannel = new NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            getString(R.string.app_name),
            NotificationManager.IMPORTANCE_DEFAULT);
        notifyMgr.createNotificationChannel(notifyChannel);

        notifyLog(
            'I',
            String.format(
                "info:\n- version name: %s\n- version code: %d\n- build type: %s\n- build time: %s",
                BuildConfig.VERSION_NAME,
                BuildConfig.VERSION_CODE,
                BuildConfig.BUILD_TYPE,
                BuildConfig.SS_BUILD_TIME));
    }

    public void notifyLog(char tag, String str) {
        Message msg = Message.obtain();
        msg.what = MSG_LOG;
        msg.arg1 = tag;
        msg.obj = str;
        mHandler.sendMessage(msg);
    }

    /** since control thread exited, should entry stopped state. */
    public void notifyControlThreadExited() {
        Message msg = Message.obtain();
        msg.what = MSG_CONTROL_THREAD_EXITED;
        mHandler.sendMessage(msg);
    }

    public static final String TAG = "ss.MainThread";
    public static final String NOTIFICATION_CHANNEL_ID = "llc.share_screen";

    /** enable unit test simulate. */
    public static final boolean ENABLE_UTSO = BuildConfig.DEBUG;

    public static final int STATE_STOPPED = 0;
    public static final int STATE_STARTING = 1;
    public static final int STATE_STARTED = 2;
    public static final int STATE_STOPPING = 3;

    public static final int MSG_LOG = 0;
    public static final int MSG_CONTROL_THREAD_EXITED = 1;

    private static MainThread sSingleton;

    public MainActivity mMainActivity = null;
    public int mState = STATE_STOPPED;
    public StringBuilder mLogCache;

    private Handler mHandler;
}