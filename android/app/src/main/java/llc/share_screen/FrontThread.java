package llc.share_screen;

import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.media.projection.MediaProjection;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

/** proxy main thread, handle ui and state manage. */
public class FrontThread extends Application {
    public static final String TAG = "my.FrontThread";
    public static final String NOTIFICATION_CHANNEL_ID = "llc.share_screen";

    private static FrontThread sSingleton;
    public static FrontThread GetSingleton() {
        return sSingleton;
    }

    private final StringBuilder mLogStr = new StringBuilder();
    private SharedPreferences mLocalData;
    private Handler mHandler;
    private MainActivity mMainActivity;

    private final Object mCloseLock = new Object();
    private boolean mIsClose = true;

    /** true mean app as init, ok to next start. */
    private boolean mIsClosed = true;

    boolean isClosed() {
        return mIsClosed;
    }

    String getLogString() {
        return mLogStr.toString();
    }

    public String readSetting() {
        return mLocalData.getString("share_screen", "");
    }

    public void writeSetting(String setting) {
        SharedPreferences.Editor e = mLocalData.edit();
        e.putString("share_screen", setting);
        e.apply();
    }

    public void setMainActivity(MainActivity activity) {
        mMainActivity = activity;
        if (mMainActivity != null) {
            mMainActivity.updateLogUi();
            mMainActivity.updateUi();
        }
    }

    /** action for button start. */
    public void opStart(MediaProjection mediaProj) {
        if (!mIsClose || !mIsClosed)
            throw new AssertionError("bug");

        mLogStr.setLength(0);
        mIsClose = false;
        mIsClosed = false;
        if (mMainActivity != null) {
            mMainActivity.updateLogUi();
            mMainActivity.updateUi();
        }

        try {
            Config cfg = new Config();
            Config.SetSingleton(cfg);
            String[] settings = readSetting().split("\\s+");
            for (String s : settings) {
                if (s.isEmpty())
                    continue;
                Integer iVal;
                if ((iVal = parseSettingInt(s, "-bit-rate=", 1, 999)) != null) {
                    cfg.bit_rate = iVal;
                } else if ((iVal = parseSettingInt(s, "-i-frame-interval=", 1, 999)) != null) {
                    cfg.i_frame_interval = iVal;
                } else if ((iVal = parseSettingInt(s, "-port=", 0, 65535)) != null) {
                    cfg.port = iVal;
                } else if ((iVal = parseSettingInt(s, "-broadcast-port=", 0, 65535)) != null) {
                    cfg.broadcast_port = iVal;
                } else if (s.equals("-debug-print-encode")) {
                    cfg.debug_print_encode = true;
                } else if (s.equals("-debug-print-net")) {
                    cfg.debug_print_net = true;
                } else {
                    throw new Error("unknown setting");
                }
            }
            // clang-format off
            log_info("current config:\nbit rate: " + cfg.bit_rate
                    + "\ni frame interval: " + cfg.i_frame_interval
                    + "\nport: " + cfg.port
                    + "\nbroadcast port: " + cfg.broadcast_port
                    + "\ndebug print encode: " + cfg.debug_print_encode
                    + "\ndebug print net: " + cfg.debug_print_net);
            // clang-format on

            BackThread backThread = new BackThread(mediaProj);
            BackThread.SetSingleton(backThread);
            NetThread netThread = new NetThread();
            NetThread.SetSingleton(netThread);
        } catch (Exception e) {
            log_err(e.getMessage());
            close();
        }
    }

    /** action for button stop. */
    public void opStop() {
        close();
    }

    public void notifyInfoLog(String str) {
        Message msg = Message.obtain();
        msg.what = MsgId.LOG_INFO.ordinal();
        msg.obj = str;
        mHandler.sendMessage(msg);
    }

    public void notifyErrLog(String str) {
        Message msg = Message.obtain();
        msg.what = MsgId.LOG_ERROR.ordinal();
        msg.obj = str;
        mHandler.sendMessage(msg);
    }

    public void notifyClosed() {
        Message msg = Message.obtain();
        msg.what = MsgId.CLOSED.ordinal();
        mHandler.sendMessage(msg);
    }

    public void close() {
        synchronized (mCloseLock) {
            if (!mIsClose) {
                mIsClose = true;
                new Thread(() -> {
                    int frameSummary = 0;
                    int existFrameSummary = 0;
                    if (BackThread.GetSingleton() != null) {
                        BackThread.GetSingleton().close();
                        int[] tmp = BackThread.GetSingleton().join();
                        frameSummary += tmp[0];
                        existFrameSummary += tmp[1];
                    }
                    if (NetThread.GetSingleton() != null) {
                        NetThread.GetSingleton().close();
                        existFrameSummary += NetThread.GetSingleton().join();
                    }
                    BackThread.ReleaseSingleton();
                    NetThread.ReleaseSingleton();
                    Config.ReleaseSingleton();

                    FrontThread.GetSingleton().notifyInfoLog("frame summary: " + frameSummary
                                                             + ", exist frame summary: "
                                                             + existFrameSummary);

                    notifyClosed();
                }).start();
            }
        }
    }

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate");

        super.onCreate();

        sSingleton = this;

        new Config();

        mLocalData = getSharedPreferences("share_screen", MODE_PRIVATE);

        mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                FrontThread.this.handleMessage(msg);
            }
        };

        NotificationManager notifyMgr =
            (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        NotificationChannel notifyChannel =
            new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                                    getString(R.string.app_name),
                                    NotificationManager.IMPORTANCE_DEFAULT);
        notifyMgr.createNotificationChannel(notifyChannel);

        log_info("version name: " + BuildConfig.VERSION_NAME + ", version code: "
                 + BuildConfig.VERSION_CODE + ", build type: " + BuildConfig.BUILD_TYPE);
    }

    private void handleMessage(Message msg) {
        switch (MsgId.From(msg.what)) {
            case LOG_INFO:
                log_info((String) msg.obj);
                break;
            case LOG_ERROR:
                log_err((String) msg.obj);
                break;
            case CLOSED:
                if (!mIsClose)
                    throw new AssertionError("bug");
                // reset to app init state.
                stopService(new Intent(this, MainService.class));
                mIsClosed = true;
                if (mMainActivity != null)
                    mMainActivity.updateUi();
                break;
        }
    }

    private Integer parseSettingInt(String setting, String prefix, int min, int max) throws Error {
        int prefixLen = prefix.length();
        if (setting.length() > prefixLen && setting.startsWith(prefix)) {
            int r;
            try {
                r = Integer.parseInt(setting.substring(prefixLen));
            } catch (NumberFormatException e) {
                throw new Error("parse setting fail, not integer, check: " + setting);
            }
            if (r < min || r > max) {
                throw new Error("parse setting fail, out of range: [" + min + ", " + max
                                + "], check: " + setting);
            }
            return r;
        }
        return null;
    }

    private void log_info(String str) {
        mLogStr.append("[INFO] ");
        mLogStr.append(str);
        mLogStr.append('\n');
        if (mMainActivity != null)
            mMainActivity.updateLogUi();
    }

    private void log_err(String str) {
        mLogStr.append("[ERR ] ");
        mLogStr.append(str);
        mLogStr.append('\n');
        if (mMainActivity != null)
            mMainActivity.updateLogUi();
    }

    ////

    private enum MsgId {
        LOG_INFO,
        LOG_ERROR,
        CLOSED;

        private static final MsgId[] sAll = MsgId.values();
        public static MsgId From(int id) {
            return sAll[id];
        }
    }
}