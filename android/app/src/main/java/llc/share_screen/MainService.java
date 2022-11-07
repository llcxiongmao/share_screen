package llc.share_screen;

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

/** foreground service. */
public class MainService extends Service {
    public static final String TAG = "my.MainService";

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate");
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "onStartCommand");

        PendingIntent openActiveIntent = PendingIntent.getActivity(
            this, 0, new Intent(this, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        Notification notify = new Notification.Builder(this, FrontThread.NOTIFICATION_CHANNEL_ID)
                                  .setSmallIcon(R.drawable.ic_launcher_foreground)
                                  .setContentText(getString(R.string.click_return_to_app))
                                  .setContentIntent(openActiveIntent)
                                  .build();
        startForeground(1, notify);

        if (FrontThread.GetSingleton().isClosed()) {
            MediaProjectionManager mgr =
                (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
            MediaProjection mediaProj = mgr.getMediaProjection(
                Activity.RESULT_OK, intent.getParcelableExtra(Intent.EXTRA_INTENT));
            FrontThread.GetSingleton().opStart(mediaProj);
        }

        return START_NOT_STICKY;
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        Log.i(TAG, "onBind");
        return null;
    }
}