package ss;

import android.app.Activity;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;

import java.util.Objects;

public class MainService extends Service {
    @Override
    public void onCreate() {
        android.util.Log.i(TAG, "onCreate");
    }

    @Override
    public void onDestroy() {
        android.util.Log.i(TAG, "onDestroy");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        android.util.Log.i(TAG, "onStartCommand");

        PendingIntent openActiveIntent = PendingIntent.getActivity(
            this, 0, new Intent(this, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        Notification notify = new Notification.Builder(this, MainThread.NOTIFICATION_CHANNEL_ID)
                                  .setSmallIcon(R.drawable.ss_notify)
                                  .setContentText(getString(R.string.click_return_to_app))
                                  .setContentIntent(openActiveIntent)
                                  .build();
        startForeground(1, notify);

        if (SessionThread.Singleton() == null) {
            MediaProjectionManager mgr =
                (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
            MediaProjection mediaProj = mgr.getMediaProjection(
                Activity.RESULT_OK,
                Objects.requireNonNull(intent.getParcelableExtra(Intent.EXTRA_INTENT)));

            MainThread.Singleton().notifyLog(
                'I',
                String.format(
                    "current config:\n- bit rate: %d\n- i interval: %d\n- port: %d\n- broadcast port: %d\n- debug encode: %s\n- debug net: %s",
                    Config.Singleton().bit_rate.value,
                    Config.Singleton().i_interval.value,
                    Config.Singleton().port.value,
                    Config.Singleton().broadcast_port.value,
                    Config.Singleton().debug_encode.value ? "true" : "false",
                    Config.Singleton().debug_net.value ? "true" : "false"));

            try {
                SessionThread controlThread = new SessionThread(mediaProj);
                SessionThread.SetSingleton(controlThread);
                MainThread.Singleton().mState = MainThread.STATE_STARTED;
                if (MainThread.Singleton().mMainActivity != null) {
                    MainThread.Singleton().mMainActivity.refreshState();
                }
            } catch (Exception e) {
                MainThread.Singleton().notifyLog('E', Log.getStackTraceString(e));
                MainThread.Singleton().mState = MainThread.STATE_STOPPED;
                if (MainThread.Singleton().mMainActivity != null) {
                    MainThread.Singleton().mMainActivity.refreshState();
                }
                return START_NOT_STICKY;
            }
        }
        return START_NOT_STICKY;
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        android.util.Log.i(TAG, "onBind");
        return null;
    }

    public static final String TAG = "ss.MainService";
}