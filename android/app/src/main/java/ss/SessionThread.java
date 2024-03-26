package ss;

import android.media.projection.MediaProjection;
import android.os.Message;
import android.util.Log;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class SessionThread {
    public static SessionThread Singleton() {
        return sSingleton;
    }

    public static void SetSingleton(SessionThread s) {
        sSingleton = s;
    }

    public SessionThread(MediaProjection mediaProj) throws Exception {
        if (sSingleton != null) {
            throw new AssertionError("bug");
        }

        mMediaProj = mediaProj;
        mQueue = new LinkedBlockingQueue<>();

        if (UTSO_FAIL_CREATE) {
            throw new Exception("unit test simulate");
        }

        Thread mThread = new Thread(this::run);
        mThread.start();
    }

    public void notifyClose() {
        mClose = true;
        Message msg = Message.obtain();
        msg.what = MSG_CLOSE;
        boolean ignored = mQueue.offer(msg);
    }

    private void run() {
        MainThread.Singleton().notifyLog('I', "session thread run");

        try {
            EncodeThread encodeThread = new EncodeThread(mMediaProj);
            EncodeThread.SetSingleton(encodeThread);
            NetThread netThread = new NetThread();
            NetThread.SetSingleton(netThread);
            while (!mClose) {
                Message msg = mQueue.poll(2000, TimeUnit.MILLISECONDS);

                if (msg == null) {
                    continue;
                }

                if (msg.what == MSG_CLOSE) {
                    msg.recycle();
                    break;
                }
            }
        } catch (Exception e) {
            MainThread.Singleton().notifyLog('E', Log.getStackTraceString(e));
        }

        if (EncodeThread.Singleton() != null) {
            EncodeThread.Singleton().notifyClose();
            EncodeThread.Singleton().join();
        }
        if (NetThread.Singleton() != null) {
            NetThread.Singleton().notifyClose();
            NetThread.Singleton().join();
        }
        EncodeThread.SetSingleton(null);
        NetThread.SetSingleton(null);

        MainThread.Singleton().notifyControlThreadExited();

        MainThread.Singleton().notifyLog('I', "session thread exit");
    }

    private static SessionThread sSingleton = null;

    /** unit test simulate options. */
    private static final boolean UTSO_FAIL_CREATE = MainThread.ENABLE_UTSO && false;

    private static final int MSG_CLOSE = 0;

    private boolean mClose = false;
    private final MediaProjection mMediaProj;
    private final BlockingQueue<Message> mQueue;
}
